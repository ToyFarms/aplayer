#include "audio_effect.h"

#include <math.h>
#include <stdlib.h>
#include <errno.h>

typedef struct audio_pan
{
    float angle;
} audio_pan;

static void eff_pan_process(audio_effect *eff, float *buf, int size)
{
    static const float mult = 1.0f / 1.4142135623730951f;

    audio_pan *ctx = eff->ctx;
    float rad = ctx->angle * (M_PI / 180.0f);
    const float l_scale = mult * (cosf(rad) + sinf(rad));
    const float r_scale = mult * (cosf(rad) - sinf(rad));

    for (int i = 0; i < size; i += 2)
    {
        buf[i] *= r_scale;
        buf[i+1] *= l_scale;
    }
}

static void eff_pan_free(audio_effect *eff)
{
    if (!eff)
        return;

    if (eff->ctx != NULL)
        free(eff->ctx);
    eff->ctx = NULL;
}

audio_effect audio_eff_pan(float angle)
{
    audio_effect eff = {0};

    eff.process = eff_pan_process;
    eff.free = eff_pan_free;
    eff.type = AUDIO_EFF_PAN;
    eff.ctx = calloc(1, sizeof(audio_pan));
    if (!eff.ctx)
    {
        errno = -ENOMEM;
        goto exit;
    }

    audio_pan *ctx = eff.ctx;
    ctx->angle = angle;

exit:
    return eff;
}

void audio_eff_pan_set(audio_effect *eff, float angle)
{
    audio_pan *ctx = eff->ctx;
    ctx->angle = angle;
}
