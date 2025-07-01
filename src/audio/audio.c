#include "audio.h"
#include "logger.h"
#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

static int audio_init_output(audio_ctx *audio)
{
    PaError err;
    WITH_MUTED_STREAM(stderr)
    {
        err = Pa_Initialize();
    }

    if (err != paNoError)
    {
        log_error("Failed to initialize PortAudio: %s\n", Pa_GetErrorText(err));
        return -1;
    }

    PaStreamParameters param;
    param.device = audio->dev < 0 ? Pa_GetDefaultOutputDevice() : audio->dev;
    audio->dev = param.device;
    param.channelCount = audio->nb_channels;
    param.sampleFormat = audio_format_to_pa_variant(audio->sample_fmt);
    assert(param.sampleFormat >= 0);
    param.suggestedLatency =
        Pa_GetDeviceInfo(param.device)->defaultLowOutputLatency;
    param.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&audio->stream, NULL, &param, audio->sample_rate, 1024,
                        paNoFlag, audio->callback, &audio->mixer);
    if (err != paNoError)
    {
        log_error("Failed to open PortAudio stream: %s\n",
                  Pa_GetErrorText(err));
        return -1;
    }

    err = Pa_StartStream(audio->stream);
    if (err != paNoError)
    {
        log_error("Failed to start PortAudio stream: %s\n",
                  Pa_GetErrorText(err));
        return -1;
    }

    return 0;
}

audio_ctx *audio_create(PaStreamCallback *callback, PaDeviceIndex dev,
                        int nb_channels, int sample_rate,
                        enum audio_format sample_fmt)
{
    audio_ctx *audio = calloc(1, sizeof(*audio));
    if (audio == NULL)
    {
        errno = -ENOMEM;
        return NULL;
    }

    audio->callback = callback;
    audio->dev = dev;
    audio->nb_channels = nb_channels;
    audio->sample_rate = sample_rate;
    audio->sample_fmt = sample_fmt;
    audio->mixer = mixer_create(nb_channels, sample_rate, sample_fmt);

    if (audio_init_output(audio) < 0)
        errno = -EINVAL;

    return audio;
}

void audio_free(audio_ctx *audio)
{
    if (audio == NULL)
        return;

    mixer_free(&audio->mixer);

    log_debug("Stopping PortAudio stream\n");
    Pa_StopStream(audio->stream);
    log_debug("Closing PortAudio stream\n");
    Pa_CloseStream(audio->stream);
    log_debug("Terminating PortAudio\n");
    Pa_Terminate();

    free(audio);
}
