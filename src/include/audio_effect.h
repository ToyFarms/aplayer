#ifndef __AUDIO_EFFECT_H
#define __AUDIO_EFFECT_H

#include "audio_callback.h"

enum audio_eff_type
{
    AUDIO_EFF_GAIN,
    AUDIO_EFF_PAN,
    AUDIO_EFF_FILTER,
};

enum audio_filt_type
{
    AUDIO_FILT_LOWPASS,
    AUDIO_FILT_HIGHPASS,
    AUDIO_FILT_BELL,
};

typedef struct audio_effect
{
    void *ctx;
    void (*process)(struct audio_effect *, audio_callback_param);
    void (*free)(struct audio_effect *);

    enum audio_eff_type type;
} audio_effect;
#define AUDIOEFF_IDX(arr, index) (((audio_effect *)(arr.data))[index])

void _audio_eff_free_default(audio_effect *eff);

audio_effect audio_eff_gain(float db);
void audio_eff_gain_set(audio_effect *eff, float db);

audio_effect audio_eff_pan(float angle);
void audio_eff_pan_set(audio_effect *eff, float angle);

typedef struct filter_bell
{
    float gain;
    float Q;
} filter_bell;

typedef struct filter_param
{
    filter_bell bell;
} filter_param;

/* param required for Bell Filter */
audio_effect audio_eff_filter(enum audio_filt_type type, float freq,
                              int sample_rate, filter_param *param);
void audio_eff_filter_set(audio_effect *eff, enum audio_filt_type type,
                          float freq, int sample_rate, filter_param *param);

#endif /* __AUDIO_EFFECT_H */
