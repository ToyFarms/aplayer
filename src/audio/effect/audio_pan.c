#include "audio_effect.h"

#include <math.h>
#include <stdlib.h>
#include <errno.h>

typedef struct effect_pan
{
    float angle;
    float gain[2];
} effect_pan;

static void eff_pan_process(audio_effect *eff, float *buf, int size, int nb_channels)
{
    if (nb_channels == 1 || nb_channels > 2)
        return;

    effect_pan *ctx = eff->ctx;

    for (int i = 0; i < size; i += nb_channels)
    {
        buf[i] *= ctx->gain[0];
        buf[i+1] *= ctx->gain[1];
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
    if (!eff.ctx)
    {
        errno = -ENOMEM;
        goto exit;
    }

    effect_pan *ctx = eff.ctx;
    ctx->angle = angle;
    update_param(ctx);

exit:
    return eff;
}

void audio_eff_pan_set(audio_effect *eff, float angle)
{
    effect_pan *ctx = eff->ctx;
    ctx->angle = angle;
    update_param(ctx);
}
