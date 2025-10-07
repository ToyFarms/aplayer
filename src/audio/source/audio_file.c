#include "_math.h"
#include "audio_source.h"
#include "image.h"
#include "imgconv.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libswresample/swresample.h"
#include "logger.h"
#include "ring_buf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct resampler
{
    SwrContext *swr;
    int nb_channels;
    int sample_rate;
    enum AVSampleFormat sample_fmt;
} resampler;

typedef struct audio_file
{
    char *filename;
    int audio_stream;

    AVFormatContext *ic;
    AVCodecContext *avctx;
    const AVCodec *codec;
    resampler resampl;

    AVFrame *frame;
    AVFrame *resampl_frame;
    AVPacket *pkt;
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

    free(ctx->filename);

    swr_free(&ctx->resampl.swr);

    log_debug("Cleanup: Closing AVFormatContext\n");
    avformat_close_input(&ctx->ic);

    log_debug("Cleanup: Free AVCodecContext\n");
    avcodec_free_context(&ctx->avctx);

    av_frame_free(&ctx->frame);
    av_frame_free(&ctx->resampl_frame);
    av_packet_free(&ctx->pkt);

    free(ctx);
    audio->ctx = NULL;

    audio_common_free(audio);
}

static int audio_resample(audio_source *audio, uint8_t **data,
                          int src_nb_samples, enum AVSampleFormat src_fmt,
                          int src_ch, int src_sample_rate,
                          enum AVSampleFormat tgt_fmt, int tgt_ch,
                          int tgt_sample_rate)
{
    audio_file *ctx = audio->ctx;

    AVChannelLayout tgt_layout;
    av_channel_layout_default(&tgt_layout, tgt_ch);

    if (ctx->resampl.swr == NULL || tgt_ch != ctx->resampl.nb_channels ||
        tgt_sample_rate != ctx->resampl.sample_rate ||
        tgt_fmt != ctx->resampl.sample_fmt)
    {
        log_debug("Reinitialization of swr context:\n    from: ch=%d sr=%d "
                  "fmt=%s\n    to: ch=%d sr=%d fmt=%s\n",
                  ctx->resampl.nb_channels, ctx->resampl.sample_rate,
                  av_get_sample_fmt_name(ctx->resampl.sample_fmt), tgt_ch,
                  tgt_sample_rate, av_get_sample_fmt_name(tgt_fmt));

        AVChannelLayout src_layout;
        av_channel_layout_default(&src_layout, src_ch);

        int ret = swr_alloc_set_opts2(&ctx->resampl.swr, &tgt_layout, tgt_fmt,
                                      tgt_sample_rate, &src_layout, src_fmt,
                                      src_sample_rate, AV_LOG_DEBUG, NULL);

        if (ret < 0 || ctx->resampl.swr == NULL ||
            swr_init(ctx->resampl.swr) < 0)
        {
            log_error("Failed to initialize SwrContext: %s\n", av_err2str(ret));
            goto fail;
        }

        ctx->resampl.nb_channels = tgt_ch;
        ctx->resampl.sample_rate = tgt_sample_rate;
        ctx->resampl.sample_fmt = tgt_fmt;
    }

    int nb_samples, max_nb_samples;
    nb_samples = max_nb_samples = av_rescale_rnd(
        swr_get_delay(ctx->resampl.swr, src_sample_rate) + src_nb_samples,
        tgt_sample_rate, src_sample_rate, AV_ROUND_UP);
    if (nb_samples <= 0)
    {
        log_error("av_rescale_rnd() error\n");
        goto fail;
    }

    int n = max_nb_samples;
    bool first_iter = true;
    do
    {
        ctx->resampl_frame->ch_layout = tgt_layout;
        ctx->resampl_frame->sample_rate = tgt_sample_rate;
        ctx->resampl_frame->format = tgt_fmt;
        ctx->resampl_frame->nb_samples = n;

        int ret = av_frame_get_buffer(ctx->resampl_frame, 0);
        if (ret < 0)
        {
            log_error("Failed to allocate sample buffer: %s\n",
                      av_err2str(ret));
            goto fail_inner;
        }
        nb_samples = ret =
            swr_convert(ctx->resampl.swr, ctx->resampl_frame->data, nb_samples,
                        first_iter ? (const uint8_t **)data : NULL,
                        first_iter ? src_nb_samples : 0);
        if (ret < 0)
        {
            log_error("Failed to resample buffer: %s\n", av_err2str(ret));
            goto fail_inner;
        }
        if (nb_samples == 0)
            goto fail_inner;

        ctx->resampl_frame->nb_samples = nb_samples;
        if (ctx->resampl_frame->nb_samples *
                ctx->resampl_frame->ch_layout.nb_channels >
            audio->buffer.capacity - audio->buffer.length)
        {
            log_error(
                "Buffer overwrite! attempt to write %d sample, available: %d\n",
                ctx->resampl_frame->nb_samples *
                    ctx->resampl_frame->ch_layout.nb_channels,
                audio->buffer.capacity - audio->buffer.length);
        }
        ret = ring_buf_write(&audio->buffer, ctx->resampl_frame->data[0],
                             ctx->resampl_frame->nb_samples *
                                 ctx->resampl_frame->ch_layout.nb_channels);
        if (ret < 0)
        {
            log_error("Failed to enqueue the audio frame\n");
            goto fail_inner;
        }
        av_frame_unref(ctx->resampl_frame);

        n -= nb_samples;
        first_iter = false;

        continue;
    fail_inner:
        av_frame_unref(ctx->resampl_frame);
        break;
    } while (n && nb_samples);

    return 0;
fail:
    return -1;
}

static int audio_file_update(audio_source *audio)
{
    audio_file *ctx = audio->ctx;
    int ret = 0, decoded_length = 0, pre_length = 0;

    if (audio->is_eof)
        return 0;

    while ((ret = avcodec_receive_frame(ctx->avctx, ctx->frame)) ==
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
            return EOF;
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

        audio->timestamp = (ctx->pkt->pts *
                            ctx->ic->streams[ctx->audio_stream]->time_base.num *
                            AV_TIME_BASE) /
                           ctx->ic->streams[ctx->audio_stream]->time_base.den;

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

    pre_length = audio->buffer.length;

    if (audio_resample(audio, ctx->frame->data, ctx->frame->nb_samples,
                       ctx->frame->format, ctx->frame->ch_layout.nb_channels,
                       ctx->frame->sample_rate, audio->target_sample_fmt,
                       audio->target_nb_channels,
                       audio->target_sample_rate) < 0)
        goto error;

    decoded_length = audio->buffer.length - pre_length;

    av_frame_unref(ctx->frame);
    return decoded_length;

error:
    av_frame_unref(ctx->frame);
    av_packet_unref(ctx->pkt);
    return -2;
}

static void audio_file_seek(audio_source *audio, int64_t ms, int whence)
{
    audio->buffer.write_idx = 0;
    audio->buffer.read_idx = 0;
    audio->buffer.length = 0;

    audio_file *file = audio->ctx;

    int64_t pos = ((double)ms / 1000.0) * (double)AV_TIME_BASE;
    int64_t abs_pos = audio->timestamp;
    switch (whence)
    {
    case SEEK_SET:
        abs_pos = pos;
        break;
    case SEEK_CUR:
        abs_pos = audio->timestamp + pos;
        break;
    case SEEK_END:
        abs_pos = audio->duration - pos;
        break;
    }

    abs_pos = MATH_CLAMP(abs_pos, 0, audio->duration);
    int err = avformat_seek_file(file->ic, -1, INT64_MIN, abs_pos, INT64_MAX,
                                 AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);

    if (err < 0)
    {
        log_error("Could not seek to %.2fs. %s.\n",
                  (double)abs_pos / (double)AV_TIME_BASE, av_err2str(err));
    }
}

static void audio_file_get_arts(audio_source *audio, array(image_t) * out)
{
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
}

static int audio_file_get_frame(audio_source *audio, int req_sample, float *out)
{
    if (req_sample < 0)
        req_sample = audio->buffer.length;

    int ret = ring_buf_read(&audio->buffer, req_sample, out);
    if (audio->is_eof && ret == -ENODATA)
        return EOF;
    else if (!audio->is_eof && ret == -ENODATA)
        return -ENODATA;

    return req_sample;
}

int audio_set_info(audio_source *audio, int nb_channels, int sample_rate,
                   enum audio_format sample_fmt)
{
    if (audio == NULL)
    {
        log_error("%s() audio is NULL\n", __FUNCTION__);
        return -1;
    }

    audio->target_nb_channels = nb_channels;
    audio->target_sample_rate = sample_rate;
    audio->target_sample_fmt = audio_format_to_av_variant(sample_fmt);
    if (audio->target_sample_fmt < 0)
    {
        log_error("Invalid sample format: %s (%d)",
                  audio_format_str(sample_fmt), (int)sample_fmt);
        return -1;
    }

    return 0;
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

    audio->stream_nb_channels = nb_channels;
    audio->stream_sample_rate = sample_rate;
    audio->stream_sample_fmt = sample_fmt;

    return 0;
}

audio_source audio_from_file(const char *filename, int nb_channels,
                             int sample_rate, enum audio_format sample_fmt)
{
    errno = 0;
    audio_source audio = {0};

    if (filename == NULL)
    {
        log_error("Failed to initialize audio: filename could not be NULL\n");
        errno = -EINVAL;
        goto exit;
    }

    audio.ctx = calloc(1, sizeof(audio_file));
    audio_file *ctx = audio.ctx;
    if (ctx == NULL)
    {
        errno = -ENOMEM;
        goto exit;
    }

    ctx->filename = strdup(filename);
    if (ctx->filename == NULL)
    {
        errno = -EINVAL;
        goto exit;
    }

    audio.is_realtime = false;
    audio.free = audio_file_free;
    audio.update = audio_file_update;
    audio.get_frame = audio_file_get_frame;
    audio.seek = audio_file_seek;
    audio.get_arts = audio_file_get_arts;

    if (audio_file_init(&audio) < 0)
    {
        log_error("Failed to initialize audio source: %s\n", filename);
        errno = -1;
        goto exit;
    }

    ctx->frame = av_frame_alloc();
    if (ctx->frame == NULL)
    {
        log_error("Failed to initialize audio: cannot allocate AVFrame\n");
        errno = -ENOMEM;
        goto exit;
    }

    ctx->resampl_frame = av_frame_alloc();
    if (ctx->resampl_frame == NULL)
    {
        log_error("Failed to initialize audio: cannot allocate AVFrame\n");
        errno = -ENOMEM;
        goto exit;
    }

    ctx->pkt = av_packet_alloc();
    if (ctx->pkt == NULL)
    {
        log_error("Failed to initialize audio: cannot allocate AVPacket\n");
        errno = -ENOMEM;
        goto exit;
    }

    if (audio_set_info(&audio, nb_channels, sample_rate, sample_fmt) < 0)
    {
        log_error("Cannot set audio info to %d:%d:%s\n", nb_channels,
                  sample_rate, audio_format_str(sample_fmt));
        errno = -EINVAL;
        goto exit;
    }

    int ret;
    if ((ret = audio_common_init(&audio)) < 0)
    {
        log_error("audio_common_init() failed with %s\n", strerror(ret));
        errno = ret;
        goto exit;
    }

exit:
    return audio;
}
