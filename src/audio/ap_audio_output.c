#include "ap_audio_output.h"

#define CASE_PLANAR(case_, ret)                                                \
    case case_:                                                                \
        return ret;                                                            \
    case case_ | paNonInterleaved:                                             \
        return ret##P

static enum AVSampleFormat pa_samplefmt_to_av_samplefmt(
    PaSampleFormat sample_fmt)
{
    switch (sample_fmt)
    {
        CASE_PLANAR(paFloat32, AV_SAMPLE_FMT_FLT);
        CASE_PLANAR(paInt32, AV_SAMPLE_FMT_S32);
        CASE_PLANAR(paInt16, AV_SAMPLE_FMT_S16);
        CASE_PLANAR(paUInt8, AV_SAMPLE_FMT_U8);
    case paInt8:
    case paInt24:
    default:
        return AV_SAMPLE_FMT_NONE;
    }
}

APAudioOutputContext *ap_audio_output_alloc(int nb_channels, int sample_rate,
                                            PaSampleFormat sample_fmt)
{
    APAudioOutputContext *audio_outctx =
        (APAudioOutputContext *)calloc(1, sizeof(APAudioOutputContext));
    if (!audio_outctx)
        return NULL;

    audio_outctx->channels = nb_channels;
    audio_outctx->sample_rate = sample_rate;
    audio_outctx->sample_fmt = sample_fmt;
    audio_outctx->av_sample_fmt = pa_samplefmt_to_av_samplefmt(sample_fmt);

    av_log(NULL, AV_LOG_DEBUG, "Initializing PortAudio\n");
    PaError pa_err = Pa_Initialize();
    if (pa_err != paNoError)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to initialize PortAudio: %s\n",
               Pa_GetErrorText(pa_err));
        ap_audio_output_free(&audio_outctx);
        return NULL;
    }

    av_log(NULL, AV_LOG_DEBUG, "Initializing PortAudio parameter\n");
    PaStreamParameters param;
    // TODO: Feature: make user select device
    param.device = Pa_GetDefaultOutputDevice();
    param.channelCount = nb_channels;
    param.sampleFormat = sample_fmt;
    param.suggestedLatency =
        Pa_GetDeviceInfo(param.device)->defaultLowOutputLatency;
    param.hostApiSpecificStreamInfo = NULL;

    av_log(NULL, AV_LOG_DEBUG, "Opening PortAudio stream\n");
    pa_err = Pa_OpenStream(&audio_outctx->stream, NULL, &param, sample_rate,
                           paFramesPerBufferUnspecified, paNoFlag, NULL, NULL);

    if (pa_err != paNoError)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open PortAudio stream: %s\n",
               Pa_GetErrorText(pa_err));
        ap_audio_output_free(&audio_outctx);
        return NULL;
    }

    if (!audio_outctx->stream)
    {
        av_log(NULL, AV_LOG_ERROR, "Null pointer: PaStream\n");
        ap_audio_output_free(&audio_outctx);
        return NULL;
    }

    av_log(NULL, AV_LOG_DEBUG, "Starting PortAudio stream\n");
    pa_err = Pa_StartStream(audio_outctx->stream);
    if (pa_err != paNoError)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to start PortAudio stream: %s\n",
               Pa_GetErrorText(pa_err));
        ap_audio_output_free(&audio_outctx);
        return NULL;
    }

    return audio_outctx;
}

void ap_audio_output_free(APAudioOutputContext **audio_outctx)
{
    PTR_PTR_CHECK(audio_outctx, );

    if ((*audio_outctx)->stream)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Stop PaStream\n");
        Pa_StopStream((*audio_outctx)->stream);
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Close PaStream\n");
        Pa_CloseStream((*audio_outctx)->stream);
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Terminate PortAudio\n");
    }
    (*audio_outctx)->stream = NULL;

    free(*audio_outctx);
    *audio_outctx = NULL;
}

void ap_audio_output_close()
{
    Pa_Terminate();
}

static uint8_t **audio_resample(uint8_t **data, int nb_samples,
                                enum AVSampleFormat src_fmt, int src_ch,
                                int src_sample_rate,
                                enum AVSampleFormat tgt_fmt, int tgt_ch,
                                int tgt_sample_rate, int *out_nb_samples);

void ap_audio_output_start_stream(APAudioOutputContext *audio_outctx,
                                  APPacketQueue *pkt_queue)
{
    while (true)
    {
        while (ap_packet_queue_empty(pkt_queue))
            av_usleep(10 * 1000);
        Packet pkt = ap_packet_queue_dequeue(pkt_queue);
        if (!pkt.frame)
            continue;

        int nb_samples = 0;
        uint8_t **audio_data = audio_resample(
            pkt.frame->data, pkt.frame->nb_samples, pkt.fmt,
            pkt.frame->ch_layout.nb_channels, pkt.frame->sample_rate,
            audio_outctx->av_sample_fmt, audio_outctx->channels,
            audio_outctx->sample_rate, &nb_samples);

        ap_audio_proc_gain((float *)(audio_data[0]), nb_samples, 0.3);
        ap_audio_proc_gain((float *)(audio_data[1]), nb_samples, 0.3);

        if (!audio_data)
        {
            av_frame_free(&pkt.frame);
            continue;
        }

        PaError pa_err =
            Pa_WriteStream(audio_outctx->stream, audio_data, nb_samples);

        av_freep(&audio_data[0]);
        av_freep(&audio_data);
        av_frame_free(&pkt.frame);

        if (pa_err == paOutputUnderflowed)
            continue;
        else if (pa_err != paNoError)
        {
            av_log(NULL, AV_LOG_ERROR, "Could not write to stream: %s\n",
                   Pa_GetErrorText(pa_err));
            break;
        }
    }
}

static uint8_t **audio_resample(uint8_t **data, int nb_samples,
                                enum AVSampleFormat src_fmt, int src_ch,
                                int src_sample_rate,
                                enum AVSampleFormat tgt_fmt, int tgt_ch,
                                int tgt_sample_rate, int *out_nb_samples)
{
    SwrContext *swr_ctx = NULL;
    uint8_t **resampled_data = NULL;

    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, tgt_ch);
    AVChannelLayout src_layout;
    av_channel_layout_default(&src_layout, src_ch);

    swr_alloc_set_opts2(&swr_ctx, &out_layout, tgt_fmt, tgt_sample_rate,
                        &src_layout, src_fmt, src_sample_rate, AV_LOG_DEBUG,
                        NULL);
    if (!swr_ctx || swr_init(swr_ctx) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to initialize SwrContext\n");
        goto fail;
    }

    int nb_samples_scaled =
        av_rescale_rnd(swr_get_delay(swr_ctx, src_sample_rate) + nb_samples,
                       tgt_sample_rate, src_sample_rate, AV_ROUND_UP);
    if (nb_samples_scaled <= 0)
    {
        av_log(NULL, AV_LOG_ERROR, "av_rescale_rnd() error\n");
        goto fail;
    }

    int linesize;
    int ret = av_samples_alloc_array_and_samples(&resampled_data, &linesize,
                                                 out_layout.nb_channels,
                                                 nb_samples_scaled, tgt_fmt, 0);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate sample buffer: %s\n",
               av_err2str(ret));
        goto fail;
    }

    ret = swr_convert(swr_ctx, resampled_data, nb_samples_scaled,
                      (const uint8_t **)data, nb_samples);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "swr_convert failed: %s\n", av_err2str(ret));
        goto fail;
    }

    if (out_nb_samples)
        *out_nb_samples = nb_samples_scaled;

    return resampled_data;

fail:
    if (swr_ctx)
        swr_free(&swr_ctx);
    if (resampled_data)
    {
        av_freep(&resampled_data[0]);
        av_freep(&resampled_data);
    }
    return NULL;
}
