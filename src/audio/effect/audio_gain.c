#include "audio_effect.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>

typedef struct effect_gain
{
    float gain;
} effect_gain;

static void eff_gain_process(audio_effect *eff, audio_callback_param p)
{
    effect_gain *ctx = eff->ctx;

    for (int i = 0; i < p.size; i++)
        p.out[i] *= ctx->gain;
}

audio_effect audio_eff_gain(float db)
{
    audio_effect eff = {0};

    eff.process = eff_gain_process;
    eff.free = _audio_eff_free_default;
    eff.type = AUDIO_EFF_GAIN;
    eff.ctx = calloc(1, sizeof(effect_gain));
    assert(eff.ctx != NULL);

    effect_gain *ctx = eff.ctx;
    ctx->gain = powf(10.0, db / 20.0);

    return eff;
}

void audio_eff_gain_set(audio_effect *eff, float db)
{
    effect_gain *ctx = eff->ctx;
    ctx->gain = powf(10.0, db / 20.0);
}
