#include "ap_audio_dec.h"

static void audiodec_fill_prop(APAudioDecoder *audioctx);
static int audio_decode(APAudioDecoder *audioctx, AVPacket *pkt,
                        AVFrame *buffer_frame, APQueue *audio_queue);
static void audio_resample(uint8_t **data, int src_nb_samples,
                           enum AVSampleFormat src_fmt, int src_ch,
                           int src_sample_rate, enum AVSampleFormat tgt_fmt,
                           int tgt_ch, int tgt_sample_rate,
                           APQueue *audio_queue);

APAudioDecoder *ap_audiodec_alloc(const char *filename, int wanted_nb_channels,
                                  int wanted_sample_rate,
                                  APAudioSampleFmt wanted_sample_fmt)
{
    av_log(NULL, AV_LOG_DEBUG, "Allocating audio context for '%s'\n", filename);
    APAudioDecoder *audiodec =
        (APAudioDecoder *)calloc(1, sizeof(APAudioDecoder));
    if (!audiodec)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate audio context\n");
        return NULL;
    }

    audiodec->filename = strdup(filename);
    audiodec->it = calloc(1, sizeof(*audiodec->it));
    audiodec->wanted_nb_channels = wanted_nb_channels;
    audiodec->wanted_sample_rate = wanted_sample_rate;
    audiodec->wanted_ap_sample_fmt = wanted_sample_fmt;
    audiodec->wanted_pa_sample_fmt =
        ap_samplefmt_to_pa_samplefmt(wanted_sample_fmt);
    if (audiodec->wanted_pa_sample_fmt < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "No equivalent format in portaudio for %s\n",
               ap_audio_samplefmt_str(wanted_sample_fmt));
        goto fail;
    }
    audiodec->wanted_av_sample_fmt =
        ap_samplefmt_to_av_samplefmt(wanted_sample_fmt);
    if (audiodec->wanted_av_sample_fmt < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "No equivalent format in ffmpeg for %s\n",
               ap_audio_samplefmt_str(wanted_sample_fmt));
        goto fail;
    }

    return audiodec;

fail:
    if (audiodec)
        ap_audiodec_free(&audiodec);
    return NULL;
}

void ap_audiodec_free(APAudioDecoder **audiodec)
{
    PTR_PTR_CHECK(audiodec, );

    ap_audiodec_stop(*audiodec);

    av_log(NULL, AV_LOG_DEBUG, "Free audio decoder for '%s'\n",
           (*audiodec)->filename);

    AudioInternalCtx *it = (*audiodec)->it;

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free AVCodecContext\n");
    avcodec_free_context(&it->avctx);

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Closing AVFormatContext\n");
    avformat_close_input(&it->ic);

    free(it);
    (*audiodec)->it = NULL;

    free((*audiodec)->filename);
    (*audiodec)->filename = NULL;

    free(*audiodec);
    *audiodec = NULL;
}

int ap_audiodec_init(APAudioDecoder *audiodec)
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing audio context\n");

    if (!audiodec->filename)
    {
        av_log(NULL, AV_LOG_ERROR, "Filename cannot be NULL\n");
        return -1;
    }

    int ret;

    av_log(NULL, AV_LOG_DEBUG, "Opening input\n");
    ret =
        avformat_open_input(&audiodec->it->ic, audiodec->filename, NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open input: %s\n",
               av_err2str(ret));
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Find stream info\n");
    av_log_set_level(AV_LOG_QUIET);
    ret = avformat_find_stream_info(audiodec->it->ic, NULL);
    av_log_set_level(AV_LOG_DEBUG);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to find stream info: %s\n",
               av_err2str(ret));
        return -1;
    }

    audiodec->it->audio_stream = av_find_best_stream(
        audiodec->it->ic, AVMEDIA_TYPE_AUDIO, -1, -1, &audiodec->it->codec, 0);

    av_log(NULL, AV_LOG_DEBUG, "Allocating AVCodecContext\n");
    audiodec->it->avctx = avcodec_alloc_context3(audiodec->it->codec);
    if (!audiodec->it->avctx)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate AVCodecContext\n");
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Opening codec\n");
    av_log_set_level(AV_LOG_QUIET);
    ret = avcodec_open2(audiodec->it->avctx, audiodec->it->codec, NULL);
    av_log_set_level(AV_LOG_DEBUG);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open codec: %s\n",
               av_err2str(ret));
        return -1;
    }

    audiodec_fill_prop(audiodec);

    return 0;
}

static void audiodec_fill_prop(APAudioDecoder *audiodec)
{
    AVCodecContext *avctx = audiodec->it->avctx;
    audiodec->channels = avctx->ch_layout.nb_channels;
    audiodec->sample_rate = avctx->sample_rate;
}

void ap_audiodec_decode(APAudioDecoder *audiodec, int max_pkt_len)
{
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    while (!audiodec->stop_signal && av_read_frame(audiodec->it->ic, pkt) >= 0)
    {
        while (!audiodec->stop_signal && max_pkt_len > 0 &&
               audiodec->audio_queue->len >= max_pkt_len)
            av_usleep(10 * 1000);

        if (pkt->stream_index == audiodec->it->audio_stream)
        {
            audio_decode(audiodec, pkt, frame, audiodec->audio_queue);
            av_frame_unref(frame);
        }

        av_packet_unref(pkt);
    }

    audiodec->stop_signal = false;
    av_packet_free(&pkt);
    av_frame_free(&frame);
}

void ap_audiodec_stop(APAudioDecoder *audiodec)
{
    audiodec->stop_signal = true;
    while (audiodec->stop_signal)
        av_usleep(10 * 1000);
}

typedef struct APAudioDecodeParams
{
    APAudioDecoder *audiodec;
    int max_pkt_len;
} APAudioDecodeParams;

static void *audiodec_decode_adapter(void *arg)
{
    APAudioDecodeParams *params = arg;
    ap_audiodec_decode(params->audiodec, params->max_pkt_len);
    free(params);
    return NULL;
}

pthread_t ap_audiodec_decode_async(APAudioDecoder *audiodec, int max_pkt_len)
{
    pthread_t tid;
    APAudioDecodeParams *params = calloc(1, sizeof(*params));
    params->audiodec = audiodec;
    params->max_pkt_len = max_pkt_len;
    pthread_create(&tid, NULL, audiodec_decode_adapter, params);

    return tid;
}

static int audio_decode(APAudioDecoder *audiodec, AVPacket *pkt,
                        AVFrame *buffer_frame, APQueue *audio_queue)
{
    avcodec_send_packet(audiodec->it->avctx, pkt);

    int ret = avcodec_receive_frame(audiodec->it->avctx, buffer_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return -1;
    else if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error during decoding: %s\n",
               av_err2str(ret));
        return -1;
    }

    audio_resample(buffer_frame->data, buffer_frame->nb_samples,
                   buffer_frame->format, buffer_frame->ch_layout.nb_channels,
                   buffer_frame->sample_rate, audiodec->wanted_av_sample_fmt,
                   audiodec->wanted_nb_channels, audiodec->wanted_sample_rate,
                   audio_queue);

    return 0;
}

static void audio_resample(uint8_t **data, int src_nb_samples,
                           enum AVSampleFormat src_fmt, int src_ch,
                           int src_sample_rate, enum AVSampleFormat tgt_fmt,
                           int tgt_ch, int tgt_sample_rate,
                           APQueue *audio_queue)
{
    // TODO: dont initialize swr_context everytime, only when the format changes
    AVChannelLayout tgt_layout;
    av_channel_layout_default(&tgt_layout, tgt_ch);
    AVChannelLayout src_layout;
    av_channel_layout_default(&src_layout, src_ch);

    SwrContext *swr_ctx = NULL;
    swr_alloc_set_opts2(&swr_ctx, &tgt_layout, tgt_fmt, tgt_sample_rate,
                        &src_layout, src_fmt, src_sample_rate, AV_LOG_DEBUG,
                        NULL);
    if (!swr_ctx || swr_init(swr_ctx) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to initialize SwrContext\n");
        goto fail;
    }

    int nb_samples, max_nb_samples;
    nb_samples = max_nb_samples =
        av_rescale_rnd(swr_get_delay(swr_ctx, src_sample_rate) + src_nb_samples,
                       tgt_sample_rate, src_sample_rate, AV_ROUND_UP);
    if (nb_samples <= 0)
    {
        av_log(NULL, AV_LOG_ERROR, "av_rescale_rnd() error\n");
        goto fail;
    }

    int iteration = 0;
    while (nb_samples)
    {
        AVFrame *frame = av_frame_alloc();
        frame->ch_layout = tgt_layout;
        frame->sample_rate = tgt_sample_rate;
        frame->format = tgt_fmt;
        frame->nb_samples = nb_samples;

        int ret = av_frame_get_buffer(frame, 0);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate sample buffer: %s\n",
                   av_err2str(ret));
            goto fail_inner;
        }

        if (iteration == 0)
            nb_samples = swr_convert(swr_ctx, frame->data, max_nb_samples,
                                     (const uint8_t **)data, src_nb_samples);
        else
            nb_samples =
                swr_convert(swr_ctx, frame->data, max_nb_samples, NULL, 0);
        if (nb_samples < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "swr_convert failed: %s\n",
                   av_err2str(ret));
            goto fail_inner;
        }
        frame->nb_samples = nb_samples;

        ap_queue_push(audio_queue, frame);
        iteration++;

        continue;

    fail_inner:
        av_frame_free(&frame);
    }

fail:
    swr_free(&swr_ctx);
}
