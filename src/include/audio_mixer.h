#ifndef __AUDIO_MIXER_H
#define __AUDIO_MIXER_H

#include "array.h"

typedef struct audio_mixer
{
    array(audio_src) sources;
} audio_mixer;

audio_mixer mixer_create();
void mixer_free(audio_mixer *mixer);
int mixer_get_frame(audio_mixer *mixer, float *out);

#endif /* __AUDIO_MIXER_H */
