#include "audio_source.h"
#include "_math.h"
#include "array.h"
#include "audio_analyzer.h"
#include "audio_effect.h"
#include "logger.h"

#include <errno.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <pthread.h>
#include <string.h>

static pthread_mutex_t id_mut = PTHREAD_MUTEX_INITIALIZER;
static uint64_t id = 0;

static uint64_t get_next_id()
{
    pthread_mutex_lock(&id_mut);
    uint64_t ret = id++;
    pthread_mutex_unlock(&id_mut);
    return ret;
}

int audio_common_init(audio_source *audio)
{
    audio->id = get_next_id();
    audio->effects = array_create(16, sizeof(audio_effect));
    audio->analyzer = array_create(16, sizeof(audio_analyzer));

    int planes =
        av_sample_fmt_is_planar(audio->sample_fmt) ? audio->nb_channels : 1;
    int channel_mult =
        av_sample_fmt_is_planar(audio->sample_fmt) ? 1 : audio->nb_channels;
    audio->planes = array_create(planes, sizeof(ring_buf_t));
    audio->markers = array_create(16, sizeof(audio_chunk_marker));

    for (int plane = 0; plane < planes; plane++)
    {
        ring_buf_t buf =
            ring_buf_create(audio->sample_rate * channel_mult,
                            av_get_bytes_per_sample(audio->sample_fmt));
        if (errno != 0)
            log_error("Cannot allocate channel %d buffer: %s\n", plane,
                      strerror(errno));
        array_append(&audio->planes, &buf, 1);
    }

    if (pthread_mutex_init(&audio->ctx_mutex, NULL) != 0)
    {
        log_error("Failed to initialize context mutex\n");
        errno = -ENOMEM;
    }

    return errno;
}

void audio_common_free(audio_source *audio)
{
    audio_effect *eff;
    ARR_FOREACH_BYREF(audio->effects, eff, i)
    {
        eff->free(eff);
    }
    array_free(&audio->effects);
    audio_analyzer *analyzer;
    ARR_FOREACH_BYREF(audio->analyzer, analyzer, i)
    {
        analyzer->free(analyzer);
    }
    array_free(&audio->analyzer);

    ring_buf_t *buf;
    ARR_FOREACH_BYREF(audio->planes, buf, i)
    {
        ring_buf_free(buf);
    }
    array_free(&audio->planes);
    array_free(&audio->markers);

    pthread_mutex_destroy(&audio->ctx_mutex);
}

int audio_common_push_frame(audio_source *audio, AVFrame *frame,
                            int64_t timestamp_us)
{
    if (av_sample_fmt_is_planar((enum AVSampleFormat)frame->format))
    {
        ring_buf_t *buf;
        ARR_FOREACH_BYREF(audio->planes, buf, ch)
        {
            if (frame->nb_samples > buf->capacity - buf->length)
            {
                log_error("Buffer overwrite! attempt to write %d sample, "
                          "available: %d (channel %d)\n",
                          frame->nb_samples, buf->capacity - buf->length, ch);
            }

            if (ring_buf_write(buf, frame->data[ch], frame->nb_samples) < 0)
            {
                log_error("Failed to enqueue the audio frame (channel %d)\n",
                          ch);
                return -1;
            }
        }
    }
    else
    {
        int nb = frame->nb_samples * frame->ch_layout.nb_channels;

        ring_buf_t *buf = &ARR_AS(audio->planes, ring_buf_t)[0];
        if (nb > buf->capacity - buf->length)
        {
            log_error("Buffer overwrite! attempt to write %d sample, "
                      "available: %d\n",
                      nb, buf->capacity - buf->length);
        }

        if (ring_buf_write(buf, frame->data[0], nb) < 0)
        {
            log_error("Failed to enqueue the audio frame\n");
            return -1;
        }
    }

    if (frame->nb_samples > 0)
    {
        audio_chunk_marker marker = {
            .nb_frames = frame->nb_samples,
            .timestamp = timestamp_us,
        };
        array_append(&audio->markers, &marker, 1);
    }

    return 0;
}

// Consumes `frames_read` frames' worth of markers from the front of the
// queue and sets audio->timestamp to the timestamp of the very first of
// those frames - i.e. the sample now being handed to the caller.
static void audio_common_consume_markers(audio_source *audio, int frames_read)
{
    int remaining = frames_read;
    bool got_start = false;

    while (remaining > 0 && audio->markers.length > 0)
    {
        audio_chunk_marker *m = &ARR_AS(audio->markers, audio_chunk_marker)[0];

        if (!got_start)
        {
            audio->timestamp = m->timestamp;
            got_start = true;
        }

        int take = MATH_MIN(remaining, m->nb_frames);
        int64_t take_us = av_rescale_q(
            take, (AVRational){1, audio->sample_rate}, AV_TIME_BASE_Q);

        m->nb_frames -= take;
        m->timestamp += take_us;
        remaining -= take;

        if (m->nb_frames == 0)
            array_remove(&audio->markers, 0, 1);
    }
}

enum audio_err audio_common_get_frame(audio_source *audio, int req_sample,
                                      void *out, int *out_samples)
{
    bool is_eof = audio->is_eof;
    int frames_read = 0;

    if (av_sample_fmt_is_planar(audio->sample_fmt))
    {
        int req_frames = req_sample < 0
                             ? ARR_AS(audio->planes, ring_buf_t)[0].length
                             : req_sample / audio->nb_channels;
        int bytes_per_sample = av_get_bytes_per_sample(audio->sample_fmt);
        size_t block_bytes = (size_t)req_frames * bytes_per_sample;

        ring_buf_t *buf;
        ARR_FOREACH_BYREF(audio->planes, buf, ch)
        {
            uint8_t *dst = (uint8_t *)out + (size_t)ch * block_bytes;
            int ret = ring_buf_read(buf, req_frames, dst);
            if (ret == -ENODATA)
            {
                if (out_samples)
                    *out_samples = 0;
                return is_eof ? AUDIO_EOF : AUDIO_NO_DATA;
            }
        }

        frames_read = req_frames;
        if (out_samples)
            *out_samples = req_frames * audio->nb_channels;
    }
    else
    {
        ring_buf_t *buf = &ARR_AS(audio->planes, ring_buf_t)[0];
        if (req_sample < 0)
            req_sample = buf->length;

        int ret = ring_buf_read(buf, req_sample, out);

        if (ret == -ENODATA)
        {
            if (out_samples)
                *out_samples = 0;
            return is_eof ? AUDIO_EOF : AUDIO_NO_DATA;
        }

        frames_read = req_sample / audio->nb_channels;
        if (out_samples)
            *out_samples = req_sample;
    }

    if (frames_read > 0)
        audio_common_consume_markers(audio, frames_read);

    return AUDIO_CONTINUE;
}
