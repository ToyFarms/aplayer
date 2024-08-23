#include "audio_effect.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>

typedef struct effect_pan
{
    float angle;
    float gain[2];
} effect_pan;

static void eff_pan_process(audio_effect *eff, audio_callback_param p)
{
    if (p.nb_channels == 1 || p.nb_channels > 2)
        return;

    effect_pan *ctx = eff->ctx;

    for (int i = 0; i < p.size; i += p.nb_channels)
    {
        p.out[i] *= ctx->gain[0];
        p.out[i + 1] *= ctx->gain[1];
    }
}

static void update_param(effect_pan *ctx)
{
    static const float mult = 1.4142135623730951f / 2.0f;
    float rad = ctx->angle * (M_PI / 180.0f);
    ctx->gain[0] = mult * (cosf(rad) + sinf(rad));
    ctx->gain[1] = mult * (cosf(rad) - sinf(rad));
}

audio_effect audio_eff_pan(float angle)
{
    audio_effect eff = {0};

    eff.process = eff_pan_process;
    eff.free = _audio_eff_free_default;
    eff.type = AUDIO_EFF_PAN;
    eff.ctx = calloc(1, sizeof(effect_pan));
    assert(eff.ctx != NULL);

    effect_pan *ctx = eff.ctx;
    ctx->angle = angle;
    update_param(ctx);

    return eff;
}

void audio_eff_pan_set(audio_effect *eff, float angle)
{
    effect_pan *ctx = eff->ctx;
    ctx->angle = angle;
    update_param(ctx);
}
