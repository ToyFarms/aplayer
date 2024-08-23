#include "audio.h"
#include "logger.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int audio_init_output(audio_ctx *audio)
{
    PaError err = Pa_Initialize();
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
    audio_ctx *audio = malloc(sizeof(*audio));

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

    Pa_StopStream(audio->stream);
    Pa_CloseStream(audio->stream);
    Pa_Terminate();

    free(audio);
}

int audio_step(audio_ctx *audio)
{
    float buffer[audio->sample_rate];
    memset(buffer, 0, sizeof(buffer));

    int frame_size = 1024;
    int ret = mixer_get_frame(&audio->mixer, frame_size, buffer);
    if (ret < 0)
        return EOF;

    ret =
        Pa_WriteStream(audio->stream, buffer, frame_size / audio->nb_channels);
    if (ret != paNoError)
        log_error("Pa_WriteStream error: %s\n", Pa_GetErrorText(ret));

    return 0;
}
