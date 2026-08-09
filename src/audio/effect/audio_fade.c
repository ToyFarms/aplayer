#include "audio_effect.h"
#include "logger.h"

#include <assert.h>
#include <stdlib.h>

typedef enum force_state
{
    FORCE_NONE,
    FORCE_IN,
    FORCE_OUT,
} force_state;

typedef struct effect_fade
{
    float fade_in_sec;
    float fade_out_sec;

    float gain;
    force_state force;
    bool force_out_done;

    int last_sample_rate;
} effect_fade;

static float max_step(float fade_sec, int sample_rate)
{
    if (fade_sec <= 0.0f || sample_rate <= 0)
        return 1.0f;

    return 1.0f / (fade_sec * (float)sample_rate);
}

static float slew_toward(float current, float target, float step)
{
    if (current < target)
    {
        current += step;
        if (current > target)
            current = target;
    }
    else if (current > target)
    {
        current -= step;
        if (current < target)
            current = target;
    }
    return current;
}

static void fade_process(audio_effect *eff, audio_callback_param p)
{
    if (p.src == NULL)
        return;

    effect_fade *ctx = eff->ctx;

    int sample_rate = p.src->target_sample_rate > 0 ? p.src->target_sample_rate
                                                    : p.src->stream_sample_rate;
    if (sample_rate <= 0)
    {
        log_error("fade: invalid sample rate, skipping\n");
        return;
    }
    ctx->last_sample_rate = sample_rate;

    int64_t time_us = p.src->timestamp;
    int64_t duration_us = p.src->duration;

    for (int i = 0; i < p.size; i += p.nb_channels)
    {
        float target;
        float step;

        if (ctx->force == FORCE_IN)
        {
            target = 1.0f;
            step = max_step(ctx->fade_in_sec, sample_rate);
        }
        else if (ctx->force == FORCE_OUT)
        {
            target = 0.0f;
            step = max_step(ctx->fade_out_sec, sample_rate);
        }
        else
        {
            /* automatic: fade in near start of track, fade out near end */
            int64_t frame_us =
                time_us +
                ((int64_t)(i / p.nb_channels) * 1000000LL) / sample_rate;

            target = 1.0f;
            step = max_step(ctx->fade_in_sec, sample_rate);

            if (ctx->fade_out_sec > 0.0f && duration_us > 0)
            {
                int64_t remaining_us = duration_us - frame_us;
                float fade_out_us = ctx->fade_out_sec * 1e6f;
                if ((float)remaining_us < fade_out_us)
                {
                    target = 0.0f;
                    step = max_step(ctx->fade_out_sec, sample_rate);
                }
            }
        }

        ctx->gain = slew_toward(ctx->gain, target, step);

        for (int ch = 0; ch < p.nb_channels; ch++)
            p.out[i + ch] *= ctx->gain;

        if (ctx->force == FORCE_OUT && ctx->gain <= 0.0f)
        {
            ctx->force_out_done = true;
        }
    }
}

static void fade_free(audio_effect *eff)
{
    _audio_eff_free_default(eff);
}

audio_effect audio_eff_fade(float fade_in_sec, float fade_out_sec)
{
    audio_effect eff = {0};

    eff.process = fade_process;
    eff.free = fade_free;
    eff.type = AUDIO_EFF_FADE;
    eff.ctx = calloc(1, sizeof(effect_fade));
    assert(eff.ctx != NULL);

    effect_fade *ctx = eff.ctx;
    ctx->fade_in_sec = fade_in_sec;
    ctx->fade_out_sec = fade_out_sec;
    ctx->gain = fade_in_sec > 0.0f ? 0.0f : 1.0f;
    ctx->force = FORCE_NONE;

    return eff;
}

void audio_eff_fade_force_in(audio_effect *eff)
{
    effect_fade *ctx = eff->ctx;
    ctx->force = FORCE_IN;
    ctx->force_out_done = false;
}

float audio_eff_fade_force_out(audio_effect *eff)
{
    effect_fade *ctx = eff->ctx;
    ctx->force = FORCE_OUT;
    ctx->force_out_done = false;
    return ctx->fade_out_sec;
}

bool audio_eff_fade_is_done(audio_effect *eff)
{
    effect_fade *ctx = eff->ctx;
    return ctx->force_out_done;
}
