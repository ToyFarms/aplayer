#include "_math.h"
#include "array.h"
#include "audio_source.h"
#include "image.h"
#include "imgconv.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libswresample/swresample.h"
#include "logger.h"
#include "metagen.h"
#include "ring_buf.h"

#include <assert.h>
#include <ebur128.h>
#include <libavutil/samplefmt.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  include <windows.h>
static inline void sleep_ms(int ms)
{
    Sleep((DWORD)ms);
}
#else
#  include <unistd.h>
static inline void sleep_ms(int ms)
{
    usleep((useconds_t)ms * 1000);
}
#endif

typedef struct audio_file
{
    char *filename;
    int audio_stream;

    AVFormatContext *ic;
    AVCodecContext *avctx;
    const AVCodec *codec;
    metadata_t *meta;

    AVFrame *frame;
    AVPacket *pkt;

    pthread_t loudness_thread;
    bool loudness_thread_started;

    pthread_mutex_t loudness_mutex;
    bool loudness_started;
    bool loudness_cancel;
    float loudness_lufs;

    int64_t next_ts_estimate;
} audio_file;

static int audio_set_stream_metadata(audio_source *audio, int nb_channels,
                                     int sample_rate,
                                     enum AVSampleFormat sample_fmt);

static int audio_file_init(audio_source *audio)
{
    log_debug("Initializing audio context\n");
    audio_file *ctx = audio->ctx;
    if (ctx == NULL)
    {
        log_error("Audio Context is NULL\n");
        return -1;
    }

    if (ctx->filename == NULL)
    {
        log_error("Filename cannot be NULL\n");
        return -1;
    }

    int ret;

    ctx->meta = metadata_read(ctx->filename);

    log_debug("Opening input\n");
    ret = avformat_open_input(&ctx->ic, ctx->filename, NULL, NULL);
    if (ret < 0)
    {
        log_error("Failed to open input: %s\n", av_err2str(ret));
        return -1;
    }
    if (ctx->ic->probe_score < 20)
    {
        log_error("Probe score too low!\n");
        return -1;
    }

    log_debug("Find stream info\n");
    av_log_set_level(AV_LOG_ERROR);
    ret = avformat_find_stream_info(ctx->ic, NULL);
    av_log_set_level(AV_LOG_DEBUG);
    if (ret < 0)
    {
        log_error("Failed to find stream info: %s\n", av_err2str(ret));
        return -1;
    }

    ctx->audio_stream = av_find_best_stream(ctx->ic, AVMEDIA_TYPE_AUDIO, -1, -1,
                                            &ctx->codec, 0);
    log_debug("Audio stream index: %d\n", ctx->audio_stream);
    if (ctx->audio_stream == AVERROR_STREAM_NOT_FOUND)
    {
        log_error("Cannot find audio stream, aborting...\n");
        return -1;
    }

    audio->duration = ctx->ic->duration;
    audio->timestamp = 0;

    log_debug("Duration: %lu\n", audio->duration);

    log_debug("Allocating AVCodecContext\n");
    ctx->avctx = avcodec_alloc_context3(ctx->codec);
    if (ctx->avctx == NULL)
    {
        log_error("Failed to allocate AVCodecContext\n");
        return -1;
    }

    log_debug("Fill codec context with paramaters from the stream\n");
    ret = avcodec_parameters_to_context(
        ctx->avctx, ctx->ic->streams[ctx->audio_stream]->codecpar);
    if (ret < 0)
    {
        log_error("Failed to convert parameters: %s\n", av_err2str(ret));
        return -1;
    }

    log_debug("Opening codec\n");
    av_log_set_level(AV_LOG_ERROR);
    ret = avcodec_open2(ctx->avctx, ctx->codec, NULL);
    av_log_set_level(AV_LOG_DEBUG);
    if (ret < 0)
    {
        log_error("Failed to open codec: %s\n", av_err2str(ret));
        return -1;
    }

    audio_set_stream_metadata(audio, ctx->avctx->ch_layout.nb_channels,
                              ctx->avctx->sample_rate, ctx->avctx->sample_fmt);

    return 0;
}

static void audio_file_free(audio_source *audio)
{
    audio_file *ctx = audio->ctx;
    if (ctx == NULL)
    {
        audio_common_free(audio);
        return;
    }

    if (ctx->loudness_thread_started)
    {
        pthread_mutex_lock(&ctx->loudness_mutex);
        ctx->loudness_cancel = true;
        pthread_mutex_unlock(&ctx->loudness_mutex);

        pthread_join(ctx->loudness_thread, NULL);
        ctx->loudness_thread_started = false;
    }

    pthread_mutex_lock(&audio->ctx_mutex);

    free(ctx->filename);

    log_debug("Cleanup: Closing AVFormatContext\n");
    avformat_close_input(&ctx->ic);

    log_debug("Cleanup: Free AVCodecContext\n");
    avcodec_free_context(&ctx->avctx);

    av_frame_free(&ctx->frame);
    av_packet_free(&ctx->pkt);

    metadata_free(ctx->meta);
    ctx->meta = NULL;

    pthread_mutex_destroy(&ctx->loudness_mutex);

    free(ctx);
    audio->ctx = NULL;

    pthread_mutex_unlock(&audio->ctx_mutex);

    audio_common_free(audio);
}

static enum audio_err audio_file_update(audio_source *audio)
{
    pthread_mutex_lock(&audio->ctx_mutex);

    audio_file *ctx = audio->ctx;
    if (ctx == NULL)
    {
        pthread_mutex_unlock(&audio->ctx_mutex);
        return AUDIO_ERROR;
    }

    int ret = 0;

    bool is_eof = audio->is_eof;
    if (is_eof)
    {
        pthread_mutex_unlock(&audio->ctx_mutex);
        return AUDIO_EOF;
    }

    ring_buf_t *buf = &ARR_AS(audio->planes, ring_buf_t)[0];
    if (buf->length > buf->capacity * 0.9)
    {
        pthread_mutex_unlock(&audio->ctx_mutex);
        return AUDIO_FULL;
    }

    while (ctx && (ret = avcodec_receive_frame(ctx->avctx, ctx->frame)) ==
                      AVERROR(EAGAIN))
    {
        ret = av_read_frame(ctx->ic, ctx->pkt);
        if (ret == AVERROR(EAGAIN))
            continue;
        else if (ret == AVERROR_EOF)
        {
            audio->is_eof = true;

            av_frame_unref(ctx->frame);
            av_packet_unref(ctx->pkt);
            pthread_mutex_unlock(&audio->ctx_mutex);
            return AUDIO_EOF;
        }
        else if (ret < 0)
        {
            log_error("av_read_frame() failed: %s\n", av_err2str(ret));
            goto error;
        }

        if (ctx->pkt->stream_index != ctx->audio_stream)
        {
            av_packet_unref(ctx->pkt);
            continue;
        }

        ret = avcodec_send_packet(ctx->avctx, ctx->pkt);
        av_packet_unref(ctx->pkt);

        if (ret == AVERROR(EAGAIN))
            continue;
        else if (ret < 0)
        {
            log_error("avcodec_send_packet() failed: %s\n", av_err2str(ret));
            goto error;
        }
    }

    if (ret < 0 && ret != AVERROR_EOF)
    {
        log_error("avcodec_receive_frame() failed: %s\n", av_err2str(ret));
        goto error;
    }

    int64_t frame_ts_us = ctx->next_ts_estimate;
    if (ctx->frame->pts != AV_NOPTS_VALUE)
    {
        AVRational stream_tb = ctx->ic->streams[ctx->audio_stream]->time_base;
        frame_ts_us = av_rescale_q(ctx->frame->pts, stream_tb, AV_TIME_BASE_Q);
    }

    ctx->next_ts_estimate =
        frame_ts_us + av_rescale_q(ctx->frame->nb_samples,
                                   (AVRational){1, ctx->frame->sample_rate},
                                   AV_TIME_BASE_Q);

    if (audio_common_push_frame(audio, ctx->frame, frame_ts_us) < 0)
        goto error;

    av_frame_unref(ctx->frame);
    pthread_mutex_unlock(&audio->ctx_mutex);
    return AUDIO_CONTINUE;

error:
    av_frame_unref(ctx->frame);
    av_packet_unref(ctx->pkt);
    pthread_mutex_unlock(&audio->ctx_mutex);
    return AUDIO_ERROR;
}

static void audio_file_seek(audio_source *audio, int64_t ms, int whence)
{
    pthread_mutex_lock(&audio->ctx_mutex);

    ring_buf_t *buf;
    ARR_FOREACH_BYREF(audio->planes, buf, i)
    {
        ring_buf_reset(buf);
    }
    audio->markers.length = 0;

    audio_file *file = audio->ctx;

    int64_t pos = ((double)ms / 1000.0) * (double)AV_TIME_BASE;

    int64_t timestamp = audio->timestamp;
    int64_t duration = audio->duration;

    int64_t abs_pos = timestamp;
    switch (whence)
    {
    case SEEK_SET:
        abs_pos = pos;
        break;
    case SEEK_CUR:
        abs_pos = timestamp + pos;
        break;
    case SEEK_END:
        abs_pos = duration - pos;
        break;
    }

    abs_pos = MATH_CLAMP(abs_pos, 0, duration);
    int err = avformat_seek_file(file->ic, -1, INT64_MIN, abs_pos, INT64_MAX,
                                 AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);

    if (err < 0)
    {
        log_error("Could not seek to %.2fs. %s.\n",
                  (double)abs_pos / (double)AV_TIME_BASE, av_err2str(err));
    }

    audio->timestamp = abs_pos;
    audio->is_eof = false;
    file->next_ts_estimate = abs_pos;
    avcodec_flush_buffers(file->avctx);

    pthread_mutex_unlock(&audio->ctx_mutex);
}

static void audio_file_get_arts(audio_source *audio, array(image_t) * out)
{
    pthread_mutex_lock(&audio->ctx_mutex);

    audio_file *file = audio->ctx;

    for (int i = 0; i < file->ic->nb_streams; i++)
    {
        AVStream *stream = file->ic->streams[i];
        if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC)
        {
            AVPacket *pkt = &stream->attached_pic;
            AVDictionaryEntry *title_ent =
                av_dict_get(stream->metadata, "title", NULL, 0);
            AVDictionaryEntry *comment_ent =
                av_dict_get(stream->metadata, "comment", NULL, 0);
            char *title = NULL;
            char *comment = NULL;
            if (title_ent)
                title = strdup(title_ent->value);
            if (comment_ent)
                comment = strdup(comment_ent->value);

            log_debug("Got attached picture: %s (%s) size=%d\n", title, comment,
                      pkt->size);
            imgconv_frame frame = imgconv_decode(
                pkt->data, pkt->size, AV_CODEC_ID_NONE, AV_PIX_FMT_RGB24);
            if (frame.buffer == NULL)
            {
                log_error("Failed to decode packet\n");
                continue;
            }

            image_t img = image_from_frame(&frame);
            img.title = title;
            img.comment = comment;

            array_append(out, &img, 1);
        }
    }

    pthread_mutex_unlock(&audio->ctx_mutex);
}

static metadata_t *audio_file_get_metadata(audio_source *audio)
{
    audio_file *ctx = audio->ctx;
    if (ctx == NULL)
        return NULL;

    return ctx->meta;
}

typedef struct loudness_work_t
{
    char *filename;
    double lufs;
    pthread_mutex_t *mutex;
    bool *cancel;
    float *result;

    bool have_hints;
    int hint_stream_index;
    int hint_sample_rate;
    int hint_nb_channels;
    enum AVCodecID hint_codec_id;
} loudness_work_t;

#define LOUDNESS_ANALYSIS_DELAY_MS 200
#define LOUDNESS_PROBE_SECONDS     2.0f
#define LOUDNESS_PROBE_COUNT       20
#define LOUDNESS_BASELINE_LUFS     -18.0f

static inline bool loudness_should_cancel(pthread_mutex_t *mutex, bool *cancel)
{
    pthread_mutex_lock(mutex);
    bool result = *cancel;
    pthread_mutex_unlock(mutex);
    return result;
}

static void loudness_do_work(void *ctx)
{
    loudness_work_t *w = ctx;
    AVCodecContext *avctx = NULL;
    ebur128_state *st = NULL;
    AVFormatContext *ic = avformat_alloc_context();
    float *buf = NULL;
    AVDictionary *open_opts = NULL;

    if (ic == NULL)
        goto done;

    if (w->have_hints)
    {
        av_dict_set(&open_opts, "probesize", "32768", 0);
        av_dict_set(&open_opts, "analyzeduration", "0", 0);
    }

    if (avformat_open_input(&ic, w->filename, NULL, &open_opts) < 0)
    {
        av_dict_free(&open_opts);
        goto done;
    }
    av_dict_free(&open_opts);

    int stream = -1;
    const AVCodec *codec = NULL;

    if (w->have_hints && w->hint_stream_index >= 0 &&
        (unsigned)w->hint_stream_index < ic->nb_streams &&
        ic->streams[w->hint_stream_index]->codecpar->codec_type ==
            AVMEDIA_TYPE_AUDIO &&
        ic->streams[w->hint_stream_index]->codecpar->sample_rate > 0)
    {
        stream = w->hint_stream_index;
        codec = avcodec_find_decoder(ic->streams[stream]->codecpar->codec_id);
    }

    if (stream < 0 || codec == NULL)
    {
        if (avformat_find_stream_info(ic, NULL) < 0)
            goto done;

        codec = NULL;
        stream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
        if (stream < 0)
            goto done;
    }

    avctx = avcodec_alloc_context3(codec);
    if (!avctx)
        goto done;
    if (avcodec_parameters_to_context(avctx, ic->streams[stream]->codecpar) < 0)
        goto done;
    if (avcodec_open2(avctx, codec, NULL) < 0)
        goto done;

    int nb_channels = avctx->ch_layout.nb_channels;
    int sample_rate = avctx->sample_rate;
    st = ebur128_init(nb_channels, sample_rate, EBUR128_MODE_I);
    if (!st)
        goto done;

    SwrContext *swr = NULL;
    AVChannelLayout src_layout, tgt_layout;
    av_channel_layout_default(&src_layout, avctx->ch_layout.nb_channels);
    av_channel_layout_default(&tgt_layout, nb_channels);
    swr_alloc_set_opts2(&swr, &tgt_layout, AV_SAMPLE_FMT_FLT, sample_rate,
                        &src_layout, avctx->sample_fmt, avctx->sample_rate, 0,
                        NULL);
    if (!swr || swr_init(swr) < 0)
    {
        swr_free(&swr);
        goto done;
    }

    int64_t duration = ic->duration;
    if (duration == AV_NOPTS_VALUE || duration <= 0)
    {
        AVStream *st_in = ic->streams[stream];
        if (st_in->duration != AV_NOPTS_VALUE && st_in->duration > 0)
        {
            duration =
                av_rescale_q(st_in->duration, st_in->time_base, AV_TIME_BASE_Q);
        }
        else
        {
            duration = AV_NOPTS_VALUE;
        }
    }

    bool sequential_mode = (duration == AV_NOPTS_VALUE);
    int64_t stride = sequential_mode ? 0 : duration / LOUDNESS_PROBE_COUNT;

    if (sequential_mode)
        log_debug("loudness: no usable duration for %s, falling back to "
                  "sequential scan\n",
                  w->filename);

    int req_samples = (int)(sample_rate * nb_channels * LOUDNESS_PROBE_SECONDS);
    buf = calloc(req_samples, sizeof(float));
    if (!buf)
    {
        swr_free(&swr);
        goto done;
    }

    AVFrame *frame = av_frame_alloc();
    AVPacket *pkt = av_packet_alloc();
    if (!frame || !pkt)
    {
        av_frame_free(&frame);
        av_packet_free(&pkt);
        swr_free(&swr);
        goto done;
    }

    bool hit_eof = false;
    int target_samples = (int)(sample_rate * LOUDNESS_PROBE_SECONDS);
    int64_t total_samples_read = 0;
    int64_t total_samples_target =
        (int64_t)target_samples * LOUDNESS_PROBE_COUNT;
    for (int probe = 0; probe < LOUDNESS_PROBE_COUNT; probe++)
    {
        if (loudness_should_cancel(w->mutex, w->cancel) || hit_eof)
            break;

        if (!sequential_mode)
        {
            int64_t target = probe * stride;
            avformat_seek_file(ic, -1, INT64_MIN, target, INT64_MAX,
                               AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(avctx);
        }

        int samples_read = 0;

        while (samples_read < target_samples)
        {
            if (loudness_should_cancel(w->mutex, w->cancel))
                break;

            int ret = av_read_frame(ic, pkt);
            if (ret == AVERROR_EOF)
            {
                hit_eof = true;
                break;
            }
            if (ret < 0)
                break;
            if (pkt->stream_index != stream)
            {
                av_packet_unref(pkt);
                continue;
            }

            avcodec_send_packet(avctx, pkt);
            av_packet_unref(pkt);

            while (avcodec_receive_frame(avctx, frame) == 0)
            {
                int out_samples = av_rescale_rnd(
                    swr_get_delay(swr, avctx->sample_rate) + frame->nb_samples,
                    sample_rate, avctx->sample_rate, AV_ROUND_UP);

                uint8_t *out_data = (uint8_t *)buf;
                int converted = swr_convert(swr, &out_data, out_samples,
                                            (const uint8_t **)frame->data,
                                            frame->nb_samples);
                if (converted > 0)
                {
                    ebur128_add_frames_float(st, buf, converted);
                    samples_read += converted;
                    total_samples_read += converted;
                }
                av_frame_unref(frame);
            }

            double measured_lufs = 0.0;
            if (ebur128_loudness_global(st, &measured_lufs) == 0 &&
                !isnan(measured_lufs) && !isinf(measured_lufs))
            {
                float weight = total_samples_target > 0
                                   ? (float)total_samples_read /
                                         (float)total_samples_target
                                   : 1.0f;
                if (weight > 1.0f)
                    weight = 1.0f;
                else if (weight < 0.0f)
                    weight = 0.0f;

                w->lufs = LOUDNESS_BASELINE_LUFS * (1.0f - weight) +
                          (float)measured_lufs * weight;
                pthread_mutex_lock(w->mutex);
                *w->result = w->lufs;
                pthread_mutex_unlock(w->mutex);
            }
        }
    }

    if (loudness_should_cancel(w->mutex, w->cancel))
        log_debug("loudness analysis cancelled\n");
    else
        log_debug("loudness: %.2f LUFS\n", w->lufs);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swr);

done:
    if (st)
        ebur128_destroy(&st);
    free(buf);
    if (ic)
        avformat_close_input(&ic);
    if (avctx)
        avcodec_free_context(&avctx);
}

static void *loudness_thread_fn(void *arg)
{
    loudness_work_t *w = arg;

    const int slice_ms = 20;
    int waited = 0;
    while (waited < LOUDNESS_ANALYSIS_DELAY_MS)
    {
        if (loudness_should_cancel(w->mutex, w->cancel))
            goto out;

        int sleep_for_ms = slice_ms < (LOUDNESS_ANALYSIS_DELAY_MS - waited)
                               ? slice_ms
                               : (LOUDNESS_ANALYSIS_DELAY_MS - waited);
        sleep_ms(sleep_for_ms);
        waited += sleep_for_ms;
    }

    if (loudness_should_cancel(w->mutex, w->cancel))
        goto out;

    loudness_do_work(w);

out:
    free(w->filename);
    free(w);
    return NULL;
}

static bool audio_start_loudness_thread(audio_source *audio)
{
    audio_file *ctx = audio->ctx;
    if (ctx == NULL)
        return false;

    pthread_mutex_lock(&ctx->loudness_mutex);
    bool already_started = ctx->loudness_started;
    ctx->loudness_started = true;
    pthread_mutex_unlock(&ctx->loudness_mutex);

    if (already_started)
        return true;

    loudness_work_t *w = calloc(1, sizeof(loudness_work_t));
    if (w == NULL)
    {
        pthread_mutex_lock(&ctx->loudness_mutex);
        ctx->loudness_started = false;
        pthread_mutex_unlock(&ctx->loudness_mutex);
        return false;
    }

    w->filename = strdup(ctx->filename);
    w->mutex = &ctx->loudness_mutex;
    w->cancel = &ctx->loudness_cancel;
    w->result = &ctx->loudness_lufs;

    if (ctx->avctx != NULL)
    {
        w->have_hints = true;
        w->hint_stream_index = ctx->audio_stream;
        w->hint_sample_rate = ctx->avctx->sample_rate;
        w->hint_nb_channels = ctx->avctx->ch_layout.nb_channels;
        w->hint_codec_id = ctx->avctx->codec_id;
    }

    if (w->filename == NULL ||
        pthread_create(&ctx->loudness_thread, NULL, loudness_thread_fn, w) != 0)
    {
        log_error("Failed to start loudness analysis thread\n");
        free(w->filename);
        free(w);
        pthread_mutex_lock(&ctx->loudness_mutex);
        ctx->loudness_started = false;
        pthread_mutex_unlock(&ctx->loudness_mutex);
        return false;
    }

    ctx->loudness_thread_started = true;
    return true;
}

static float audio_get_loudness(audio_source *audio)
{
    audio_file *ctx = audio->ctx;
    if (ctx == NULL)
        return 0.0f;

    audio_start_loudness_thread(audio);

    pthread_mutex_lock(&ctx->loudness_mutex);
    float lufs = ctx->loudness_lufs;
    pthread_mutex_unlock(&ctx->loudness_mutex);
    return lufs;
}

static int audio_set_stream_metadata(audio_source *audio, int nb_channels,
                                     int sample_rate,
                                     enum AVSampleFormat sample_fmt)
{
    if (audio == NULL)
    {
        log_error("audio_source is NULL\n");
        return -1;
    }

    audio->nb_channels = nb_channels;
    audio->sample_rate = sample_rate;
    audio->sample_fmt = sample_fmt;

    return 0;
}

audio_source audio_from_file(const char *filename)
{
    errno = 0;
    audio_source audio = {0};
    audio_file *ctx = NULL;
    int ret = 0;

    if (filename == NULL)
    {
        log_error("Failed to initialize audio: filename could not be NULL\n");
        errno = -EINVAL;
        goto fail;
    }

    audio.ctx = calloc(1, sizeof(audio_file));
    ctx = audio.ctx;
    if (ctx == NULL)
    {
        errno = -ENOMEM;
        goto fail;
    }

    ctx->filename = strdup(filename);
    if (ctx->filename == NULL)
    {
        errno = -EINVAL;
        goto fail;
    }

    if (pthread_mutex_init(&ctx->loudness_mutex, NULL) != 0)
    {
        log_error("Failed to initialize audio: cannot init loudness mutex\n");
        errno = -1;
        goto fail;
    }

    audio.is_realtime = false;
    audio.free = audio_file_free;
    audio.update = audio_file_update;
    audio.get_frame = audio_common_get_frame;
    audio.seek = audio_file_seek;
    audio.get_arts = audio_file_get_arts;
    audio.get_metadata = audio_file_get_metadata;
    audio.get_loudness = audio_get_loudness;

    if (audio_file_init(&audio) < 0)
    {
        log_error("Failed to initialize audio source: %s\n", filename);
        errno = -1;
        goto fail;
    }

    ctx->frame = av_frame_alloc();
    if (ctx->frame == NULL)
    {
        log_error("Failed to initialize audio: cannot allocate AVFrame\n");
        errno = -ENOMEM;
        goto fail;
    }

    ctx->pkt = av_packet_alloc();
    if (ctx->pkt == NULL)
    {
        log_error("Failed to initialize audio: cannot allocate AVPacket\n");
        errno = -ENOMEM;
        goto fail;
    }

    if ((ret = audio_common_init(&audio)) < 0)
    {
        log_error("audio_common_init() failed with %s\n", strerror(ret));
        pthread_mutex_destroy(&audio.ctx_mutex);
        errno = ret;
        goto fail;
    }

    audio_start_loudness_thread(&audio);

    return audio;

fail:
    audio_file_free(&audio);
    return audio;
}
