#ifndef __AUDIO_MIXER_H
#define __AUDIO_MIXER_H

#include "array.h"
#include "audio_format.h"

#include <pthread.h>

typedef struct audio_mixer
{
    int nb_channels;
    int sample_rate;
    enum audio_format sample_fmt;

    pthread_mutex_t source_mutex;
    array(audio_source) sources;
    array(plugin_module) master_plugins;
} audio_mixer;

audio_mixer mixer_create(int nb_channels, int sample_rate,
                         enum audio_format sample_fmt);
void mixer_free(audio_mixer *mixer);
int mixer_get_frame(audio_mixer *mixer, int req_sample, float *out);

#endif /* __AUDIO_MIXER_H */
