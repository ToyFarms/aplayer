#include "audio.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libswresample/swresample.h"
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
    ring_buf_t audio_buffer;

    AVFormatContext *ic;
    AVCodecContext *avctx;
    const AVCodec *codec;
    resampler resampl;

    AVFrame *frame;
    AVPacket *pkt;
} audio_file;

static int audio_set_stream_metadata(audio_src *audio, int nb_channels,
                                     int sample_rate,
                                     enum AVSampleFormat sample_fmt);

static int audio_file_init(audio_src *audio)
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing audio context\n");
    audio_file *ctx = audio->ctx;
    if (ctx == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "Audio Context is NULL\n");
        return -1;
    }

    if (ctx->filename == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "Filename cannot be NULL\n");
        return -1;
    }

    int ret;

    av_log(NULL, AV_LOG_DEBUG, "Opening input\n");
    ret = avformat_open_input(&ctx->ic, ctx->filename, NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open input: %s\n",
               av_err2str(ret));
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Find stream info\n");
    av_log_set_level(AV_LOG_ERROR);
    ret = avformat_find_stream_info(ctx->ic, NULL);
    av_log_set_level(AV_LOG_DEBUG);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to find stream info: %s\n",
               av_err2str(ret));
        return -1;
    }

    ctx->audio_stream = av_find_best_stream(ctx->ic, AVMEDIA_TYPE_AUDIO, -1, -1,
                                            &ctx->codec, 0);

    av_log(NULL, AV_LOG_DEBUG, "Allocating AVCodecContext\n");
    ctx->avctx = avcodec_alloc_context3(ctx->codec);
    if (ctx->avctx == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate AVCodecContext\n");
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG,
           "Fill codec context with paramaters from the stream\n");
    ret = avcodec_parameters_to_context(
        ctx->avctx, ctx->ic->streams[ctx->audio_stream]->codecpar);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to convert parameters: %s\n",
               av_err2str(ret));
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Opening codec\n");
    av_log_set_level(AV_LOG_ERROR);
    ret = avcodec_open2(ctx->avctx, ctx->codec, NULL);
    av_log_set_level(AV_LOG_DEBUG);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open codec: %s\n",
               av_err2str(ret));
        return -1;
    }

    audio_set_stream_metadata(audio, ctx->avctx->ch_layout.nb_channels,
                              ctx->avctx->sample_rate, ctx->avctx->sample_fmt);

    return 0;
}

static void audio_file_free(audio_src *audio)
{
    audio_file *ctx = audio->ctx;
    if (ctx == NULL)
        return;

    free(ctx->filename);

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free AVCodecContext\n");
    avcodec_free_context(&ctx->avctx);

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Closing AVFormatContext\n");
    avformat_close_input(&ctx->ic);

    av_frame_free(&ctx->frame);
    av_packet_free(&ctx->pkt);
    ring_buf_free(&ctx->audio_buffer);

    free(ctx);
    audio->ctx = NULL;
}

static int audio_resample(audio_src *audio, uint8_t **data, int src_nb_samples,
                          enum AVSampleFormat src_fmt, int src_ch,
                          int src_sample_rate, enum AVSampleFormat tgt_fmt,
                          int tgt_ch, int tgt_sample_rate)
{
    audio_file *ctx = audio->ctx;

    AVChannelLayout tgt_layout;
    av_channel_layout_default(&tgt_layout, tgt_ch);

    if (ctx->resampl.swr == NULL || tgt_ch != ctx->resampl.nb_channels ||
        tgt_sample_rate != ctx->resampl.sample_rate ||
        tgt_fmt != ctx->resampl.sample_fmt)
    {
        av_log(NULL, AV_LOG_DEBUG,
               "Reinitialization of swr context:\n    from: ch=%d sr=%d "
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
            av_log(NULL, AV_LOG_ERROR, "Failed to initialize SwrContext: %s\n",
                   av_err2str(ret));
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
        av_log(NULL, AV_LOG_ERROR, "av_rescale_rnd() error\n");
        goto fail;
    }

    int n = max_nb_samples;
    bool first_iter = true;
    do
    {
        AVFrame *frame = av_frame_alloc();
        if (!frame)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate AVFrame\n");
            goto fail_inner;
        }

        frame->ch_layout = tgt_layout;
        frame->sample_rate = tgt_sample_rate;
        frame->format = tgt_fmt;
        frame->nb_samples = n;

        int ret = av_frame_get_buffer(frame, 0);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate sample buffer: %s\n",
                   av_err2str(ret));
            goto fail_inner;
        }
        nb_samples = ret =
            swr_convert(ctx->resampl.swr, frame->data, nb_samples,
                        first_iter ? (const uint8_t **)data : NULL,
                        first_iter ? src_nb_samples : 0);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to resample buffer: %s\n",
                   av_err2str(ret));
            goto fail_inner;
        }
        if (nb_samples == 0)
            goto fail_inner;

        frame->nb_samples = nb_samples;
        ret = ring_buf_write(&ctx->audio_buffer, frame->data[0],
                             frame->nb_samples * frame->ch_layout.nb_channels);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to enqueue the audio frame\n");
            goto fail_inner;
        }
        av_frame_free(&frame);

        n -= nb_samples;
        first_iter = false;

        continue;
    fail_inner:
        av_frame_free(&frame);
        break;
    } while (n && nb_samples);

    return 0;
fail:
    return -1;
}

static int audio_update(audio_src *audio)
{
    audio_file *ctx = audio->ctx;
    if (audio->is_finished)
        return 0;
    if (ctx->audio_buffer.length >= audio->stream_sample_rate * 5)
        return 0;

    int ret;

    while (avcodec_receive_frame(ctx->avctx, ctx->frame) == AVERROR(EAGAIN))
    {
        ret = av_read_frame(ctx->ic, ctx->pkt);
        if (ret == AVERROR(EAGAIN))
            continue;
        else if (ret == AVERROR_EOF)
        {
            audio->is_finished = true;
            return EOF;
        }
        else if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "av_read_frame() failed: %s\n",
                   av_err2str(ret));
            goto error;
        }

        if (ctx->pkt->stream_index != ctx->audio_stream)
            continue;

        ret = avcodec_send_packet(ctx->avctx, ctx->pkt);
        if (ret == AVERROR(EAGAIN))
            continue;
        else if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "avcodec_send_packet() failed: %s\n",
                   av_err2str(ret));
            goto error;
        }
    }

    if (audio_resample(audio, ctx->frame->data, ctx->frame->nb_samples,
                       ctx->frame->format, ctx->frame->ch_layout.nb_channels,
                       ctx->frame->sample_rate, audio->target_sample_fmt,
                       audio->target_nb_channels,
                       audio->target_sample_rate) < 0)
        goto error;

    av_frame_unref(ctx->frame);
    av_packet_unref(ctx->pkt);
    return 0;

error:
    av_frame_unref(ctx->frame);
    av_packet_unref(ctx->pkt);
    return -1;
}

static int audio_get_frame(audio_src *audio, int req_sample, float *out)
{
    audio_file *ctx = audio->ctx;
    char buf[1000];
    int len = snprintf(buf, 1000, "\rread=%-10d write=%-10d length=%-10d",
                       ctx->audio_buffer.read_idx, ctx->audio_buffer.write_idx,
                       ctx->audio_buffer.length);
    write(STDOUT_FILENO, buf, len);

    int ret = ring_buf_read(&ctx->audio_buffer, req_sample, out);
    if (audio->is_finished && ret == -ENODATA)
        return EOF;

    return ret;
}

int audio_set_metadata(audio_src *audio, int nb_channels, int sample_rate,
                       enum audio_format sample_fmt)
{
    if (audio == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "%s() audio is NULL\n", __FUNCTION__);
        return -1;
    }

    audio->target_nb_channels = nb_channels;
    audio->target_sample_rate = sample_rate;
    audio->target_sample_fmt = audio_format_to_av_variant(sample_fmt);
    if (audio->target_sample_fmt < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "%s() Invalid sample format: %s (%d)",
               __FUNCTION__, audio_format_str(sample_fmt), (int)sample_fmt);
        return -1;
    }

    return 0;
}

static int audio_set_stream_metadata(audio_src *audio, int nb_channels,
                                     int sample_rate,
                                     enum AVSampleFormat sample_fmt)
{
    if (audio == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "%s() audio is NULL\n", __FUNCTION__);
        return -1;
    }

    audio->stream_nb_channels = nb_channels;
    audio->stream_sample_rate = sample_rate;
    audio->stream_sample_fmt = sample_fmt;

    return 0;
}

audio_src audio_from_file(const char *filename)
{
    audio_src audio = {0};

    if (filename == NULL)
    {
        av_log(NULL, AV_LOG_ERROR,
               "Failed to initialize audio: filename could not be NULL\n");
        errno = -EINVAL;
        goto exit;
    }

    audio.ctx = calloc(1, sizeof(audio_file));
    audio_file *ctx = audio.ctx;

    ctx->filename = strdup(filename);
    assert(ctx->filename != NULL);

    audio.free = audio_file_free;
    audio.update = (void *)audio_update;
    audio.get_frame = audio_get_frame;

    ctx->audio_buffer = ring_buf_create(48000 * 10, sizeof(float));

    if (audio_file_init(&audio) < 0)
        av_log(NULL, AV_LOG_ERROR, "Failed to initialize audio source: %s\n",
               filename);

    AVFrame *frame = av_frame_alloc();
    if (frame == NULL)
    {
        av_log(NULL, AV_LOG_ERROR,
               "Failed to initialize audio: cannot allocate AVFrame\n");
        errno = -ENOMEM;
        goto exit;
    }
    ctx->frame = frame;

    AVPacket *pkt = av_packet_alloc();
    if (pkt == NULL)
    {
        av_log(NULL, AV_LOG_ERROR,
               "Failed to initialize audio: cannot allocate AVPacket\n");
        errno = -ENOMEM;
        goto exit;
    }
    ctx->pkt = pkt;

exit:
    return audio;
}
