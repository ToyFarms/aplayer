#ifndef __AUDIO_MIXER_H
#define __AUDIO_MIXER_H

#include "array.h"
#include "audio_format.h"
#include "audio_resampler.h"
#include "dict.h"

#include <pthread.h>

typedef struct audio_source_resampler
{
    resample_ctx_t rate_fmt;
    resample_ctx_t channel_mix;
    array(float) leftover;
} audio_source_resampler;

typedef struct audio_mixer
{
    int nb_channels;
    int sample_rate;
    enum audio_format sample_fmt;

    pthread_mutex_t source_mutex;
    array(audio_source) sources;

    array(uint8_t) raw_scratch;
    array(float) scratch;

    // audio_source.id -> audio_source_resampler *
    dict_t resamplers;

    float master_gain;
    float norm_gain;
    bool paused;
    bool muted;
    array(audio_effect) effects;
    array(audio_analyzer) analyzer;
} audio_mixer;

audio_mixer mixer_create(int nb_channels, int sample_rate,
                         enum audio_format sample_fmt);
void mixer_free(audio_mixer *mixer);
void mixer_clear(audio_mixer *mixer);
int mixer_get_frame(audio_mixer *mixer, int req_sample, float *out);

#endif /* __AUDIO_MIXER_H */
