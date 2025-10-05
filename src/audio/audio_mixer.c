#include "audio_mixer.h"
#include "audio_analyzer.h"
#include "audio_effect.h"
#include "audio_source.h"
#include "logger.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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
    pthread_mutex_unlock(&mixer->source_mutex);
}

int mixer_get_frame(audio_mixer *mixer, int req_sample, float *out)
{
    int ret = 0, len = 0, max_len = 0;
    if (mixer->paused)
        return 0;

    pthread_mutex_lock(&mixer->source_mutex);
    audio_source *src;
    float gain = powf(10, mixer->master_gain / 20);
    ARR_FOREACH_BYREF(mixer->sources, src, i)
    {
        if (src->is_finished)
            continue;

        while (!src->is_eof && src->buffer.length < req_sample)
            ret = src->update(src);

        mixer->scratch.length = 0;
        ret = len = src->get_frame(src, req_sample, mixer->scratch.data);
        if (len > max_len)
            max_len = len;

        if (ret == -ENODATA)
        {
            log_error("Stream have no data left\n");
            continue;
        }
        else if (ret == EOF)
        {
            src->is_finished = true;
            ret = len = src->get_frame(src, -1, mixer->scratch.data);
            log_error("Stream finished, flushing leftover (%d sample)\n", len);
            src->free(src);
            array_remove(&mixer->sources, i, 1);
        }

        assert(mixer->scratch.capacity >= len);
        for (int sample = 0; sample < len; sample++)
            out[sample] += ARR_AS(mixer->scratch, float)[sample];
    }

    // TODO: fix order, make all changeable

    float peak = 0.0f;
    for (int sample = 0; sample < max_len; sample++)
    {
        float a = fabsf(out[sample]);
        if (a > peak)
            peak = a;
    }

    const float target_peak = 0.95f;
    float desired_gain = 1.0f;
    if (peak > 1e-12f)
        desired_gain = (peak > target_peak) ? (target_peak / peak) : 1.0f;

    const float attack = 0.2f;
    const float release = 0.99f;

    if (mixer->norm_gain <= 0.0f)
        mixer->norm_gain = 1.0f;

    if (desired_gain < mixer->norm_gain)
        mixer->norm_gain =
            mixer->norm_gain * (1.0f - attack) + desired_gain * attack;
    else
        mixer->norm_gain =
            mixer->norm_gain * release + desired_gain * (1.0f - release);

    for (int sample = 0; sample < max_len; sample++)
        out[sample] *= mixer->norm_gain;

    audio_effect *eff;
    ARR_FOREACH_BYREF(mixer->effects, eff, i)
    {
        eff->process(eff, AUDIO_CALLBACK_PARAM(out, max_len, mixer->nb_channels,
                                               mixer->sample_rate,
                                               mixer->sample_fmt));
    }

    for (int sample = 0; sample < max_len; sample++)
        out[sample] *= gain;

    audio_analyzer *analyzer;
    ARR_FOREACH_BYREF(mixer->analyzer, analyzer, i)
    {
        analyzer->process(
            analyzer, (audio_callback_param){.out = out,
                                             .size = max_len,
                                             .nb_channels = mixer->nb_channels,
                                             .sample_rate = mixer->sample_rate,
                                             .sample_fmt = mixer->sample_fmt});
    }

    pthread_mutex_unlock(&mixer->source_mutex);

    return 0;
}
