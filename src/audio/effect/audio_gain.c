#include "audio_effect.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct audio_gain
{
    float gain;
} audio_gain;

static void eff_gain_process(audio_effect *eff, float *buf, int size)
{
    audio_gain *ctx = eff->ctx;
    float lin_scale = powf(10.0, ctx->gain / 20.0);

    for (int i = 0; i < size; i++)
        buf[i] *= lin_scale;
}

static void eff_gain_free(audio_effect *eff)
{
    if (!eff)
        return;

    if (eff->ctx != NULL)
        free(eff->ctx);
    eff->ctx = NULL;
}

audio_effect audio_eff_gain(float db)
{
    audio_effect eff = {0};

    eff.process = eff_gain_process;
    eff.free = eff_gain_free;
    eff.type = AUDIO_EFF_GAIN;
    eff.ctx = calloc(1, sizeof(audio_gain));
    if (!eff.ctx)
    {
        errno = -ENOMEM;
        goto exit;
    }

    audio_gain *ctx = eff.ctx;
    ctx->gain = db;

exit:
    return eff;
}

void audio_eff_gain_set(audio_effect *eff, float db)
{
    audio_gain *ctx = eff->ctx;
    ctx->gain = db;
}
