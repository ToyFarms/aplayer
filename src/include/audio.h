#ifndef __AUDIO_H
#define __AUDIO_H

#include "portaudio.h"
#include "audio_format.h"
#include "audio_mixer.h"

typedef struct audio_ctx
{
    PaStream *stream;
    audio_mixer mixer;

    PaDeviceIndex dev;
    int nb_channels;
    int sample_rate;
    enum audio_format sample_fmt;
    PaStreamCallback *callback;
} audio_ctx;

audio_ctx *audio_create(PaStreamCallback *callback, PaDeviceIndex dev,
                       int nb_channels, int sample_rate,
                       enum audio_format sample_fmt);
void audio_free(audio_ctx *ctx);
int audio_step(audio_ctx *audio);

#endif /* __AUDIO_H */
