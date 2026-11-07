#include "audio_analyzer.h"

#include <math.h>
#include <stdlib.h>

#define ANALYZER_ACTIVITY_BAND_LOW_HZ         300.0f
#define ANALYZER_ACTIVITY_BAND_HIGH_HZ        3400.0f
#define ANALYZER_ACTIVITY_DEFAULT_ATTACK_MS   0.0f
#define ANALYZER_ACTIVITY_DEFAULT_DECAY_MS    50.0f
#define ANALYZER_ACTIVITY_DEFAULT_CURVE       0.75f
#define ANALYZER_ACTIVITY_DEFAULT_GAIN        4.0f
#define ANALYZER_ACTIVITY_DEFAULT_SMOOTHING   0.1f
#define ANALYZER_ACTIVITY_DEFAULT_SENSITIVITY 50.0f

typedef struct analyzer_activity
{
    float level[ANALYZER_ACTIVITY_MAX_CHANNELS];
    float smoothed[ANALYZER_ACTIVITY_MAX_CHANNELS];
    bool primed[ANALYZER_ACTIVITY_MAX_CHANNELS];
    float attack_ms;
    float decay_ms;
    float curve;
    float gain;
    float smoothing;
    float sensitivity;

    float hp_prev_in[ANALYZER_ACTIVITY_MAX_CHANNELS];
    float hp_prev_out[ANALYZER_ACTIVITY_MAX_CHANNELS];
    float lp_prev_out[ANALYZER_ACTIVITY_MAX_CHANNELS];

    float hp_coef;
    float lp_coef;
    int filter_sample_rate;
} analyzer_activity;

void activity_free(audio_analyzer *analyzer)
{
    free(analyzer->ctx);
    analyzer->ctx = NULL;
}

void activity_process(audio_analyzer *analyzer, audio_callback_param p)
{
    analyzer_activity *ctx = analyzer->ctx;

    int nb_channels = p.nb_channels;
    if (nb_channels > ANALYZER_ACTIVITY_MAX_CHANNELS)
        nb_channels = ANALYZER_ACTIVITY_MAX_CHANNELS;

    if (nb_channels <= 0 || p.nb_channels <= 0 || p.sample_rate <= 0)
        return;

    int nb_samples = p.size / p.nb_channels;
    if (nb_samples <= 0)
        return;

    float block_ms = 1000.0f * (float)nb_samples / (float)p.sample_rate;

    float attack_coef = (ctx->attack_ms <= 0.0f)
                            ? 1.0f
                            : 1.0f - expf(-block_ms / ctx->attack_ms);
    float decay_coef = 1.0f - expf(-block_ms / ctx->decay_ms);

    if (ctx->filter_sample_rate != p.sample_rate)
    {
        float dt = 1.0f / (float)p.sample_rate;
        float rc_hp =
            1.0f / (2.0f * 3.141592653f * ANALYZER_ACTIVITY_BAND_LOW_HZ);
        float rc_lp =
            1.0f / (2.0f * 3.141592653f * ANALYZER_ACTIVITY_BAND_HIGH_HZ);

        ctx->hp_coef = rc_hp / (rc_hp + dt);
        ctx->lp_coef = dt / (rc_lp + dt);
        ctx->filter_sample_rate = p.sample_rate;

        for (int ch = 0; ch < ANALYZER_ACTIVITY_MAX_CHANNELS; ch++)
        {
            ctx->hp_prev_in[ch] = 0.0f;
            ctx->hp_prev_out[ch] = 0.0f;
            ctx->lp_prev_out[ch] = 0.0f;
        }
    }

    float peak[ANALYZER_ACTIVITY_MAX_CHANNELS] = {0};

    for (int i = 0; i < p.size; i += p.nb_channels)
    {
        for (int ch = 0; ch < nb_channels; ch++)
        {
            float x = p.out[i + ch];

            float hp_out = ctx->hp_coef *
                           (ctx->hp_prev_out[ch] + x - ctx->hp_prev_in[ch]);
            ctx->hp_prev_in[ch] = x;
            ctx->hp_prev_out[ch] = hp_out;

            float lp_out = ctx->lp_prev_out[ch] +
                           ctx->lp_coef * (hp_out - ctx->lp_prev_out[ch]);
            ctx->lp_prev_out[ch] = lp_out;

            float s = fabsf(lp_out);
            if (s > peak[ch])
                peak[ch] = s;
        }
    }

    for (int ch = 0; ch < nb_channels; ch++)
    {
        float raw = peak[ch] * ctx->gain;
        if (raw > 1.0f)
            raw = 1.0f;
        if (isnan(raw))
            raw = 0.0f;

        float prev_smoothed = ctx->primed[ch] ? ctx->smoothed[ch] : raw;
        float new_smoothed =
            prev_smoothed + (raw - prev_smoothed) * ctx->smoothing;

        float delta = fabsf(new_smoothed - prev_smoothed) * ctx->sensitivity;
        if (delta > 1.0f)
            delta = 1.0f;

        float target = powf(delta, ctx->curve);

        ctx->smoothed[ch] = new_smoothed;
        ctx->primed[ch] = true;

        float coef = (target > ctx->level[ch]) ? attack_coef : decay_coef;

        ctx->level[ch] += (target - ctx->level[ch]) * coef;
        if (isnan(ctx->level[ch]))
            ctx->level[ch] = 0.0f;
    }

    analyzer_activity_ctx out_ctx = {.nb_channels = nb_channels};
    for (int ch = 0; ch < nb_channels; ch++)
        out_ctx.level[ch] = ctx->level[ch];

    analyzer->callback(&out_ctx, analyzer->userdata);
}

audio_analyzer audio_analyzer_activity(analyzer_callback callback,
                                       void *userdata)
{
    audio_analyzer analyzer = {0};

    analyzer.type = AUDIO_ANALYZER_ACTIVITY;
    analyzer.callback = callback;
    analyzer.userdata = userdata;
    analyzer.free = activity_free;
    analyzer.process = activity_process;
    analyzer.ctx = calloc(1, sizeof(analyzer_activity));

    analyzer_activity *ctx = analyzer.ctx;
    ctx->attack_ms = ANALYZER_ACTIVITY_DEFAULT_ATTACK_MS;
    ctx->decay_ms = ANALYZER_ACTIVITY_DEFAULT_DECAY_MS;
    ctx->curve = ANALYZER_ACTIVITY_DEFAULT_CURVE;
    ctx->gain = ANALYZER_ACTIVITY_DEFAULT_GAIN;
    ctx->smoothing = ANALYZER_ACTIVITY_DEFAULT_SMOOTHING;
    ctx->sensitivity = ANALYZER_ACTIVITY_DEFAULT_SENSITIVITY;

    return analyzer;
}
