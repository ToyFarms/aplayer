#include "audio_effect.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

#define PAN_DELAY_BUF_SIZE   256
#define ITD_MAX_SEC          0.00066f
#define HEAD_SHADOW_STRENGTH 0.7f
#define LPF_MIN_CUTOFF       1200.0f
#define LPF_MAX_CUTOFF       20000.0f

typedef struct effect_pan
{
    float angle;

    float gain[2];
    float shadow_gain[2];

    int delay_samples[2];
    float delay_buf[2][PAN_DELAY_BUF_SIZE];
    int delay_pos[2];

    float lpf_coef[2];
    float lpf_state[2];

    int sample_rate;
} effect_pan;

static void update_param(effect_pan *ctx, int sample_rate)
{
    static const float mult = 1.4142135623730951f / 2.0f;
    float rad = ctx->angle * ((float)M_PI / 180.0f);

    ctx->gain[0] = mult * (cosf(rad) + sinf(rad));
    ctx->gain[1] = mult * (cosf(rad) - sinf(rad));

    float x = sinf(rad);

    ctx->shadow_gain[0] = 1.0f - HEAD_SHADOW_STRENGTH * fmaxf(0.0f, x);
    ctx->shadow_gain[1] = 1.0f - HEAD_SHADOW_STRENGTH * fmaxf(0.0f, -x);

    float delay_l_sec = ITD_MAX_SEC * fmaxf(0.0f, x);
    float delay_r_sec = ITD_MAX_SEC * fmaxf(0.0f, -x);
    ctx->delay_samples[0] = (int)(delay_l_sec * sample_rate + 0.5f);
    ctx->delay_samples[1] = (int)(delay_r_sec * sample_rate + 0.5f);
    if (ctx->delay_samples[0] >= PAN_DELAY_BUF_SIZE)
        ctx->delay_samples[0] = PAN_DELAY_BUF_SIZE - 1;
    if (ctx->delay_samples[1] >= PAN_DELAY_BUF_SIZE)
        ctx->delay_samples[1] = PAN_DELAY_BUF_SIZE - 1;

    float cutoff_l =
        LPF_MAX_CUTOFF - (LPF_MAX_CUTOFF - LPF_MIN_CUTOFF) * fmaxf(0.0f, x);
    float cutoff_r =
        LPF_MAX_CUTOFF - (LPF_MAX_CUTOFF - LPF_MIN_CUTOFF) * fmaxf(0.0f, -x);
    ctx->lpf_coef[0] =
        1.0f - expf(-2.0f * (float)M_PI * cutoff_l / sample_rate);
    ctx->lpf_coef[1] =
        1.0f - expf(-2.0f * (float)M_PI * cutoff_r / sample_rate);

    ctx->sample_rate = sample_rate;
}

static void eff_pan_process(audio_effect *eff, audio_callback_param p)
{
    if (p.nb_channels == 1 || p.nb_channels > 2)
        return;

    effect_pan *ctx = eff->ctx;

    if (p.sample_rate != ctx->sample_rate)
        update_param(ctx, p.sample_rate);

    for (int i = 0; i < p.size; i += p.nb_channels)
    {
        for (int ch = 0; ch < 2; ch++)
        {
            float in = p.out[i + ch];

            ctx->delay_buf[ch][ctx->delay_pos[ch]] = in;
            int read_pos = ctx->delay_pos[ch] - ctx->delay_samples[ch];
            if (read_pos < 0)
                read_pos += PAN_DELAY_BUF_SIZE;
            float delayed = ctx->delay_buf[ch][read_pos];
            ctx->delay_pos[ch] = (ctx->delay_pos[ch] + 1) % PAN_DELAY_BUF_SIZE;

            ctx->lpf_state[ch] +=
                ctx->lpf_coef[ch] * (delayed - ctx->lpf_state[ch]);

            p.out[i + ch] =
                ctx->lpf_state[ch] * ctx->gain[ch] * ctx->shadow_gain[ch];
        }
    }
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
    update_param(ctx, 48000);
    return eff;
}

void audio_eff_pan_set(audio_effect *eff, float angle)
{
    effect_pan *ctx = eff->ctx;
    ctx->angle = angle;
    update_param(ctx, ctx->sample_rate);
}
