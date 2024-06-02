#include "ap_audio_output.h"

#include "libavutil/frame.h"
#include "libavutil/time.h"

int ap_audio_output_init(APAudioOutputContext *ctx, int nb_channels,
                         int sample_rate, APAudioSampleFmt sample_fmt)
{
    ctx->channels = nb_channels;
    ctx->sample_rate = sample_rate;
    ctx->ap_sample_fmt = sample_fmt;
    ctx->pa_sample_fmt = ap_samplefmt_to_pa_samplefmt(sample_fmt);
    if (ctx->pa_sample_fmt < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "No equivalent format in portaudio for %s\n",
               ap_audio_samplefmt_str(sample_fmt));
        goto fail;
    }
    ctx->av_sample_fmt = ap_samplefmt_to_av_samplefmt(sample_fmt);
    if (ctx->av_sample_fmt < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "No equivalent format in ffmpeg for %s\n",
               ap_audio_samplefmt_str(sample_fmt));
        goto fail;
    }

    av_log(NULL, AV_LOG_DEBUG, "Initializing PortAudio\n");
    PaError pa_err = Pa_Initialize();
    if (pa_err != paNoError)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to initialize PortAudio: %s\n",
               Pa_GetErrorText(pa_err));
        goto fail;
    }

    av_log(NULL, AV_LOG_DEBUG, "Initializing PortAudio parameter\n");
    PaStreamParameters param;
    // TODO: Feature: make user select device
    param.device = Pa_GetDefaultOutputDevice();
    param.channelCount = nb_channels;
    param.sampleFormat = ctx->pa_sample_fmt;
    param.suggestedLatency =
        Pa_GetDeviceInfo(param.device)->defaultLowOutputLatency;
    param.hostApiSpecificStreamInfo = NULL;

    av_log(NULL, AV_LOG_DEBUG, "Opening PortAudio stream\n");
    pa_err = Pa_OpenStream(&ctx->stream, NULL, &param, sample_rate,
                           paFramesPerBufferUnspecified, paClipOff, NULL, NULL);

    if (pa_err != paNoError)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open PortAudio stream: %s\n",
               Pa_GetErrorText(pa_err));
        goto fail;
    }

    if (!ctx->stream)
    {
        av_log(NULL, AV_LOG_ERROR, "Null pointer: PaStream\n");
        goto fail;
    }

    av_log(NULL, AV_LOG_DEBUG, "Starting PortAudio stream\n");
    pa_err = Pa_StartStream(ctx->stream);
    if (pa_err != paNoError)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to start PortAudio stream: %s\n",
               Pa_GetErrorText(pa_err));
        goto fail;
    }

    return 0;

fail:
    ap_audio_output_free(&ctx);

    return -1;
}

APAudioOutputContext *ap_audio_output_alloc(int nb_channels, int sample_rate,
                                            APAudioSampleFmt sample_fmt)
{
    APAudioOutputContext *audio_outctx = calloc(1, sizeof(*audio_outctx));
    if (!audio_outctx)
        return NULL;

    ap_audio_output_init(audio_outctx, nb_channels, sample_rate, sample_fmt);

    return audio_outctx;
}

void ap_audio_output_free(APAudioOutputContext **audio_outctx)
{
    PTR_PTR_CHECK(audio_outctx, );

    ap_audio_output_stop(*audio_outctx);

    if ((*audio_outctx)->stream)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Stop PaStream\n");
        Pa_StopStream((*audio_outctx)->stream);
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Close PaStream\n");
        Pa_CloseStream((*audio_outctx)->stream);
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Terminate Portaudio\n");
        Pa_Terminate();
    }
    (*audio_outctx)->stream = NULL;

    free(*audio_outctx);
    *audio_outctx = NULL;
}

void ap_audio_output_start(APAudioOutputContext *audio_outctx,
                           APQueue *audio_queue)
{
    while (!audio_outctx->stop_signal)
    {
        while (!audio_outctx->stop_signal && audio_queue &&
               AP_QUEUE_IS_EMPTY(audio_queue))
            av_usleep(10 * 1000);
        if (audio_outctx->stop_signal)
            break;

        AVFrame *frame = ap_queue_pop(audio_queue);
        if (!frame)
        {
            av_log(NULL, AV_LOG_ERROR, "Audio Queue returns NULL pointer\n");
            continue;
        }

        PaError ret = Pa_WriteStream(audio_outctx->stream, frame->data,
                                     frame->nb_samples);
        if (ret == paOutputUnderflowed)
            ;
        else if (ret != paNoError)
            av_log(NULL, AV_LOG_ERROR, "Pa_WriteStream error: %s\n",
                   Pa_GetErrorText(ret));

        av_frame_free(&frame);
    }

    audio_outctx->stop_signal = false;
}

void ap_audio_output_stop(APAudioOutputContext *audio_outctx)
{
    audio_outctx->stop_signal = true;
    while (audio_outctx->stop_signal)
        av_usleep(10 * 1000);
}

typedef struct AudioOutputParams
{
    APAudioOutputContext *ctx;
    APQueue *audio_queue;
} AudioOutputParams;

static void *audio_output_start_adapter(void *arg)
{
    AudioOutputParams *params = arg;
    ap_audio_output_start(params->ctx, params->audio_queue);
    free(params);
    return NULL;
}

pthread_t ap_audio_output_start_async(APAudioOutputContext *audio_outctx,
                                      APQueue *audio_queue)
{
    pthread_t tid;
    AudioOutputParams *params = calloc(1, sizeof(*params));
    params->ctx = audio_outctx;
    params->audio_queue = audio_queue;
    pthread_create(&tid, NULL, audio_output_start_adapter, params);

    return tid;
}
