#ifndef __AUDIO_CALLBACK_H
#define __AUDIO_CALLBACK_H

#include "audio_format.h"

typedef struct audio_callback_param
{
    float *out;
    int size;
    int nb_channels;
    int sample_rate;
    enum audio_format sample_fmt;
} audio_callback_param;

#define AUDIO_CALLBACK_PARAM(buf, size, nb_channels, sample_rate, sample_fmt)  \
    (audio_callback_param)                                                     \
    {                                                                          \
        buf, size, nb_channels, sample_rate, sample_fmt                        \
    }

#endif /* __AUDIO_CALLBACK_H */
