#include "audio_mixer.h"
#include "array.h"
#include "audio_analyzer.h"
#include "audio_effect.h"
#include "audio_resampler.h"
#include "audio_source.h"
#include "dict.h"
#include "logger.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BYTES_PER_SAMPLE 8
#define MAX_PLANES           32

static void audio_source_resampler_free(void *data)
{
    audio_source_resampler *r = data;
    if (r == NULL)
        return;

    resample_ctx_free(&r->rate_fmt);
    resample_ctx_free(&r->channel_mix);
    array_free(&r->leftover);
    free(r);
}

static pthread_mutex_t a = PTHREAD_MUTEX_INITIALIZER;

static audio_source_resampler *mixer_get_resampler(audio_mixer *mixer,
                                                   audio_source *src)
{
    pthread_mutex_lock(&a);
    char key[32] = {0};
    snprintf(key, sizeof(key), "%" PRIu64, src->id);

    audio_source_resampler *r = dict_get(&mixer->resamplers, key, NULL);
    if (r != NULL)
    {
        pthread_mutex_unlock(&a);
        return r;
    }

    r = calloc(1, sizeof(*r));
    if (r == NULL)
    {
        log_error("Cannot allocate resampler for source id %" PRIu64 "\n",
                  src->id);
        pthread_mutex_unlock(&a);
        return NULL;
    }

    if (resample_ctx_init(&r->rate_fmt) < 0 ||
        resample_ctx_init(&r->channel_mix) < 0)
    {
        log_error("Cannot initialize resampler for source id %" PRIu64 "\n",
                  src->id);
        audio_source_resampler_free(r);
        pthread_mutex_unlock(&a);
        return NULL;
    }

    r->leftover = array_create(mixer->nb_channels * 256, sizeof(float));

    dict_insert(&mixer->resamplers, key, r);
    pthread_mutex_unlock(&a);
    return r;
}

static int mixer_scratch_sink(void *sink_ctx, AVFrame *frame)
{
    array_t *arr = sink_ctx;
    int nb = frame->nb_samples * frame->ch_layout.nb_channels;
    array_append(arr, frame->data[0], nb);

    return 0;
}

typedef struct mix_sink_ctx
{
    float *out;
    int out_offset;
    int out_capacity;
    array_t *leftover;
} mix_sink_ctx;

static void mixer_accumulate(float *out, int *out_offset, int out_capacity,
                             const float *src, int nb, array_t *leftover)
{
    int remaining = out_capacity - *out_offset;
    int to_write = nb < remaining ? nb : remaining;

    for (int i = 0; i < to_write; i++)
        out[*out_offset + i] += src[i];

    *out_offset += to_write;

    if (nb > to_write)
    {
        if (leftover != NULL)
        {
            array_append(leftover, src + to_write, nb - to_write);
        }
        else
        {
            log_error("Mixer output overwrite! attempt to write %d sample, "
                      "available: %d\n",
                      nb, remaining);
        }
    }
}

static int mixer_accumulate_sink(void *sink_ctx, AVFrame *frame)
{
    mix_sink_ctx *ctx = sink_ctx;
    int nb = frame->nb_samples * frame->ch_layout.nb_channels;

    mixer_accumulate(ctx->out, &ctx->out_offset, ctx->out_capacity,
                     (const float *)frame->data[0], nb, ctx->leftover);

    return 0;
}

static void mixer_finish_source(audio_mixer *mixer, int index,
                                audio_source *src, bool flushing)
{
    if (!flushing)
        return;

    src->free(src);
    array_remove(&mixer->sources, index, 1);
}

audio_mixer mixer_create(int nb_channels, int sample_rate,
                         enum audio_format sample_fmt)
{
    // TODO: add support for planar audio, idk about non float sample format
    assert(AUDIO_IS_PLANAR(sample_fmt) == false && sample_fmt == AUDIO_FLT);

    audio_mixer mixer = {0};

    mixer.nb_channels = nb_channels;
    mixer.sample_rate = sample_rate;
    mixer.sample_fmt = sample_fmt;
    mixer.master_gain = 0.0f;
    mixer.analyzer = array_create(4, sizeof(audio_analyzer));
    mixer.effects = array_create(4, sizeof(audio_effect));
    pthread_mutex_init(&mixer.source_mutex, NULL);

    mixer.sources = array_create(16, sizeof(audio_source));
    if (errno != 0)
        log_error("Cannot allocate mixer sources: %s\n", strerror(errno));

    mixer.scratch = array_create(sample_rate * nb_channels, sizeof(float));
    if (errno != 0)
        log_error("Cannot allocate mixer scratch buffer: %s\n",
                  strerror(errno));

    mixer.raw_scratch =
        array_create(sample_rate * nb_channels * MAX_BYTES_PER_SAMPLE, 1);
    if (errno != 0)
        log_error("Cannot allocate mixer raw scratch buffer: %s\n",
                  strerror(errno));

    mixer.resamplers = dict_create();
    mixer.resamplers.free = audio_source_resampler_free;

    return mixer;
}

void mixer_free(audio_mixer *mixer)
{
    if (mixer == NULL)
        return;

    pthread_mutex_lock(&mixer->source_mutex);
    audio_analyzer *analyzer;
    ARR_FOREACH_BYREF(mixer->analyzer, analyzer, i)
    {
        analyzer->free(analyzer);
    }
    audio_effect *eff;
    ARR_FOREACH_BYREF(mixer->effects, eff, i)
    {
        eff->free(eff);
    }
    pthread_mutex_unlock(&mixer->source_mutex);

    mixer_clear(mixer);

    pthread_mutex_lock(&mixer->source_mutex);
    array_free(&mixer->analyzer);
    array_free(&mixer->effects);
    array_free(&mixer->sources);
    array_free(&mixer->scratch);
    array_free(&mixer->raw_scratch);
    dict_free(&mixer->resamplers);
    pthread_mutex_unlock(&mixer->source_mutex);

    pthread_mutex_destroy(&mixer->source_mutex);
}

void mixer_clear(audio_mixer *mixer)
{
    pthread_mutex_lock(&mixer->source_mutex);
    audio_source *src;
    ARR_FOREACH_BYREF(mixer->sources, src, i)
    {
        src->free(src);
    }
    mixer->sources.length = 0;
    dict_clear(&mixer->resamplers);
    pthread_mutex_unlock(&mixer->source_mutex);
}

int mixer_get_frame(audio_mixer *mixer, int req_sample, float *out)
{
    int max_len = 0;
    if (mixer->paused)
        return 0;

    pthread_mutex_lock(&mixer->source_mutex);

    float master_gain = powf(10, mixer->master_gain / 20);

    audio_source *src;
    ARR_FOREACH_BYREF(mixer->sources, src, i)
    {
        if (src->is_finished)
            continue;

        bool flushing = false;

        audio_source_resampler *rs = mixer_get_resampler(mixer, src);
        if (rs == NULL)
            continue;

        int out_offset = 0;

        if (rs->leftover.length > 0)
        {
            mixer_accumulate(out, &out_offset, req_sample,
                             (const float *)rs->leftover.data,
                             rs->leftover.length, NULL);

            if (out_offset < (int)rs->leftover.length)
            {
                memmove(rs->leftover.data,
                        (float *)rs->leftover.data + out_offset,
                        (rs->leftover.length - out_offset) * sizeof(float));
                rs->leftover.length -= out_offset;

                if (out_offset > max_len)
                    max_len = out_offset;

                continue;
            }

            rs->leftover.length = 0;
        }

        int req_frames = (req_sample - out_offset) / mixer->nb_channels;
        if (req_frames <= 0)
            continue;
        int src_req_frames = (int)ceil((double)req_frames * src->sample_rate /
                                       (double)mixer->sample_rate) +
                             1;
        int src_req_sample = src_req_frames * src->nb_channels;

        enum audio_err act = AUDIO_CONTINUE;
        while ((act = src->update(src)) == AUDIO_CONTINUE)
        {
        }

        if (act == AUDIO_ERROR)
        {
            log_error("Failed to update source\n");
            continue;
        }

        int bytes_per_sample = av_get_bytes_per_sample(src->sample_fmt);
        array_ensure_size(&mixer->raw_scratch,
                          src_req_sample * bytes_per_sample);

        int nb_read = 0;
        enum audio_err retcode = src->get_frame(
            src, src_req_sample, mixer->raw_scratch.data, &nb_read);

        if (retcode == AUDIO_EOF)
        {
            src->is_finished = true;
            flushing = true;
            retcode =
                src->get_frame(src, -1, mixer->raw_scratch.data, &nb_read);
            log_error("Stream finished, flushing leftover (%d sample)\n",
                      nb_read);
        }

        if (retcode == AUDIO_ERROR)
        {
            log_error("Failed to read frame from source\n");
            mixer_finish_source(mixer, i, src, flushing);
            continue;
        }

        if (retcode == AUDIO_NO_DATA || nb_read == 0)
        {
            mixer_finish_source(mixer, i, src, flushing);
            continue;
        }

        bool src_planar = av_sample_fmt_is_planar(src->sample_fmt);
        int frame_count = nb_read / src->nb_channels;

        mixer->scratch.length = 0;
        if (!src_planar && src->sample_fmt == AV_SAMPLE_FMT_FLT &&
            src->sample_rate == mixer->sample_rate)
        {
            array_append(&mixer->scratch, mixer->raw_scratch.data, nb_read);
        }
        else
        {
            const uint8_t *planes[MAX_PLANES];
            if (src_planar)
            {
                if (src->nb_channels > MAX_PLANES)
                {
                    log_error("Too many channels (%d) for planar source, "
                              "max supported is %d\n",
                              src->nb_channels, MAX_PLANES);
                    mixer_finish_source(mixer, i, src, flushing);
                    continue;
                }

                int bytes_per_sample = av_get_bytes_per_sample(src->sample_fmt);
                for (int c = 0; c < src->nb_channels; c++)
                    planes[c] = (const uint8_t *)mixer->raw_scratch.data +
                                (size_t)c * frame_count * bytes_per_sample;
            }
            else
            {
                planes[0] = mixer->raw_scratch.data;
            }

            if (resample_convert(&rs->rate_fmt, planes, frame_count,
                                 src->sample_fmt, src->nb_channels,
                                 src->sample_rate, AV_SAMPLE_FMT_FLT,
                                 src->nb_channels, mixer->sample_rate,
                                 mixer_scratch_sink, &mixer->scratch) < 0)
            {
                log_error("Failed to resample source to mixer sample rate\n");
                mixer_finish_source(mixer, i, src, flushing);
                continue;
            }
        }

        if (mixer->muted)
        {
            mixer_finish_source(mixer, i, src, flushing);
            continue;
        }

        audio_effect *eff;
        ARR_FOREACH_BYREF(src->effects, eff, j)
        {
            eff->process(eff, AUDIO_CALLBACK_PARAM(
                                  src, ARR_AS(mixer->scratch, float),
                                  mixer->scratch.length, src->nb_channels,
                                  mixer->sample_rate, AUDIO_FLT));
        }

        audio_analyzer *analyzer;
        ARR_FOREACH_BYREF(src->analyzer, analyzer, j)
        {
            analyzer->process(
                analyzer,
                AUDIO_CALLBACK_PARAM(NULL, ARR_AS(mixer->scratch, float),
                                     mixer->scratch.length, src->nb_channels,
                                     mixer->sample_rate, AUDIO_FLT));
        }

        if (src->nb_channels == mixer->nb_channels)
        {
            mixer_accumulate(out, &out_offset, req_sample,
                             (const float *)mixer->scratch.data,
                             mixer->scratch.length, &rs->leftover);
        }
        else
        {
            const uint8_t *float_plane = (const uint8_t *)mixer->scratch.data;
            mix_sink_ctx mix_ctx = {
                .out = out,
                .out_offset = out_offset,
                .out_capacity = req_sample,
                .leftover = &rs->leftover,
            };

            int written = resample_convert(
                &rs->channel_mix, &float_plane,
                mixer->scratch.length / src->nb_channels, AV_SAMPLE_FMT_FLT,
                src->nb_channels, mixer->sample_rate, AV_SAMPLE_FMT_FLT,
                mixer->nb_channels, mixer->sample_rate, mixer_accumulate_sink,
                &mix_ctx);

            if (written < 0)
                log_error("Failed to mix source channels into mixer layout\n");

            out_offset = mix_ctx.out_offset;
        }

        if (out_offset > max_len)
            max_len = out_offset;

        mixer_finish_source(mixer, i, src, flushing);
    }

    // TODO: fix order, make all changeable

    if (max_len > 0)
    {
        audio_effect *eff;
        ARR_FOREACH_BYREF(mixer->effects, eff, i)
        {
            eff->process(eff, AUDIO_CALLBACK_PARAM(
                                  NULL, out, max_len, mixer->nb_channels,
                                  mixer->sample_rate, mixer->sample_fmt));
        }

        for (int sample = 0; sample < max_len; sample++)
            out[sample] *= master_gain;

        audio_analyzer *analyzer;
        ARR_FOREACH_BYREF(mixer->analyzer, analyzer, i)
        {
            analyzer->process(
                analyzer,
                AUDIO_CALLBACK_PARAM(NULL, out, max_len, mixer->nb_channels,
                                     mixer->sample_rate, mixer->sample_fmt));
        }
    }

    pthread_mutex_unlock(&mixer->source_mutex);

    return 0;
}
