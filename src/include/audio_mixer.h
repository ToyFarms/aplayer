#ifndef __AUDIO_MIXER_H
#define __AUDIO_MIXER_H

#include "array.h"
#include "audio_format.h"

typedef struct audio_mixer
{
    int nb_channels;
    int sample_rate;
    enum audio_format sample_fmt;

    array(audio_src) sources;
    array(audio_effect) master_effect;
    array(audio_analyzer) master_analyzer;
} audio_mixer;

audio_mixer mixer_create(int nb_channels, int sample_rate,
                         enum audio_format sample_fmt);
void mixer_free(audio_mixer *mixer);
int mixer_get_frame(audio_mixer *mixer, float *out);

#endif /* __AUDIO_MIXER_H */
