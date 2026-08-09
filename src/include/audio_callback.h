#ifndef __AUDIO_CALLBACK_H
#define __AUDIO_CALLBACK_H

#include "audio_format.h"
#include "audio_source.h"

typedef struct audio_callback_param
{
    /* may be NULL; */
    audio_source *src;

    float *out;
    int size;
    int nb_channels;
    int sample_rate;
    enum audio_format sample_fmt;
} audio_callback_param;

#define AUDIO_CALLBACK_PARAM(src, buf, size, nb_channels, sample_rate,         \
                             sample_fmt)                                       \
    (audio_callback_param)                                                     \
    {                                                                          \
        src, buf, size, nb_channels, sample_rate, sample_fmt                   \
    }

#endif /* __AUDIO_CALLBACK_H */
