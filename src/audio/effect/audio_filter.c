#include "audio_effect.h"
#include "logger.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct bell_filter
{
    float b0, b1, b2, a0, a1, a2;
    float x1, x2, y1, y2;
} bell_filter;

typedef struct pass_filter
{
    float prev_filtered[8];
    float prev_sample[8];
    bool initialized;
} pass_filter;

typedef struct effect_filter
{
    enum audio_filt_type type;
    float freq;
    int sample_rate;

    float alpha;
    bell_filter bell;
    pass_filter pass;
} effect_filter;

static void filter_lowpass(effect_filter *filter, float *out, int size,
                           int nb_channels)
{
    pass_filter *pass = &filter->pass;

    if (!pass->initialized)
    {
        for (int ch = 0; ch < nb_channels; ch++)
            pass->prev_filtered[ch] = out[ch];
        pass->initialized = true;
    }

    for (int i = 0; i < size; i += nb_channels)
    {
        for (int ch = 0; ch < nb_channels; ch++)
        {
            out[i + ch] =
                pass->prev_filtered[ch] +
                (filter->alpha * (out[i + ch] - pass->prev_filtered[ch]));
            pass->prev_filtered[ch] = out[i + ch];
        }
    }
}

static void filter_highpass(effect_filter *filter, float *out, int size,
                            int nb_channels)
{
    pass_filter *pass = &filter->pass;

    if (!pass->initialized)
    {
        for (int ch = 0; ch < nb_channels; ch++)
        {
            pass->prev_filtered[ch] = out[ch];
            pass->prev_sample[ch] = out[ch];
        }
        pass->initialized = true;
    }

    for (int i = 0; i < size; i += nb_channels)
    {
        for (int ch = 0; ch < nb_channels; ch++)
        {
            float sample = out[i + ch];
            out[i + ch] = filter->alpha * pass->prev_filtered[ch] +
                          filter->alpha * (sample - pass->prev_sample[ch]);
            pass->prev_filtered[ch] = out[i + ch];
            pass->prev_sample[ch] = sample;
        }
    }
}

static void _filter_bell(effect_filter *filter, float *out, int size,
                         int nb_channels)
{
    bell_filter *bell = &filter->bell;

    for (int i = 0; i < size; i += nb_channels)
    {
        for (int ch = 0; ch < nb_channels; ch++)
        {
            float sample = out[i + ch];
            float sample_out = bell->b0 * sample + bell->b1 * bell->x1 +
                               bell->b2 * bell->x2 - bell->a1 * bell->y1 -
                               bell->a2 * bell->y2;
            bell->x2 = bell->x1;
            bell->x1 = sample;
            bell->y2 = bell->y1;
            bell->y1 = sample_out;

            out[i + ch] = sample_out;
        }
    }
}

static void update_param(effect_filter *filter, filter_param *param)
{
    if (filter->type == AUDIO_FILT_LOWPASS)
    {
        float RC = 1.0 / (filter->freq * 2.0 * M_PI);
        float dt = 1.0 / filter->sample_rate;
        filter->alpha = dt / (RC + dt);
    }
    else if (filter->type == AUDIO_FILT_HIGHPASS)
    {
        float RC = 1.0 / (filter->freq * 2.0 * M_PI);
        float dt = 1.0 / filter->sample_rate;
        filter->alpha = RC / (RC + dt);
    }
    else if (filter->type == AUDIO_FILT_BELL)
    {
        assert(param != NULL);

        bell_filter *bell = &filter->bell;
        float omega = 2.0f * M_PI * filter->freq / filter->sample_rate;
        float alpha = sinf(omega) / (2.0f * param->bell.Q);
        float A = powf(10, param->bell.gain / 40);

        bell->b0 = 1 + alpha * A;
        bell->b1 = -2 * cosf(omega);
        bell->b2 = 1 - alpha * A;
        bell->a0 = 1 + alpha / A;
        bell->a1 = -2 * cosf(omega);
        bell->a2 = 1 - alpha / A;

        bell->b0 /= bell->a0;
        bell->b1 /= bell->a0;
        bell->b2 /= bell->a0;
        bell->a1 /= bell->a0;
        bell->a2 /= bell->a0;

        bell->x1 = bell->x2 = bell->y1 = bell->y2 = 0.0f;
    }
}

void eff_filter_proccess(audio_effect *eff, audio_callback_param p)
{
    effect_filter *ctx = eff->ctx;
    enum audio_filt_type type = ctx->type;
    if (type == AUDIO_FILT_LOWPASS)
        filter_lowpass(eff->ctx, p.out, p.size, p.nb_channels);
    else if (type == AUDIO_FILT_HIGHPASS)
        filter_highpass(eff->ctx, p.out, p.size, p.nb_channels);
    else if (type == AUDIO_FILT_BELL)
        _filter_bell(eff->ctx, p.out, p.size, p.nb_channels);
    else
        log_error("Unknown filter type: %d\n", type);
}

audio_effect audio_eff_filter(enum audio_filt_type type, float freq,
                              int sample_rate, filter_param *param)
{
    audio_effect eff = {0};

    eff.process = eff_filter_proccess;
    eff.free = _audio_eff_free_default;
    eff.type = AUDIO_EFF_FILTER;
    eff.ctx = calloc(1, sizeof(effect_filter));
    assert(eff.ctx != NULL);

    effect_filter *ctx = eff.ctx;
    ctx->type = type;
    ctx->freq = freq;
    ctx->sample_rate = sample_rate;

    update_param(ctx, param);

    return eff;
}

void audio_eff_filter_set(audio_effect *eff, enum audio_filt_type type,
                          float freq, int sample_rate, filter_param *param)
{
    effect_filter *ctx = eff->ctx;
    ctx->type = type;
    ctx->freq = freq;
    ctx->sample_rate = sample_rate;

    update_param(ctx, param);
}
