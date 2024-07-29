#include "audio_effect.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>

typedef struct effect_gain
{
    float gain;
} effect_gain;

static void eff_gain_process(audio_effect *eff, float *buf, int size, int nb_channels)
{
    effect_gain *ctx = eff->ctx;
    float lin_scale = powf(10.0, ctx->gain / 20.0);

    for (int i = 0; i < size; i++)
        buf[i] *= lin_scale;
}

audio_effect audio_eff_gain(float db)
{
    audio_effect eff = {0};

    eff.process = eff_gain_process;
    eff.free = _audio_eff_free_default;
    eff.type = AUDIO_EFF_GAIN;
    eff.ctx = calloc(1, sizeof(effect_gain));
    if (!eff.ctx)
    {
        errno = -ENOMEM;
        goto exit;
    }

    effect_gain *ctx = eff.ctx;
    ctx->gain = db;

exit:
    return eff;
}

void audio_eff_gain_set(audio_effect *eff, float db)
{
    effect_gain *ctx = eff->ctx;
    ctx->gain = db;
}
