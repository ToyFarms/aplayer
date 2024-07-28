#include "audio_effect.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct bell_filter
{
    float b0, b1, b2, a0, a1, a2;
    float x1, x2, y1, y2;
} bell_filter;

typedef struct audio_filter
{
    enum audio_filt_type type;
    float freq;
    int nb_channels;
    int sample_rate;

    float alpha;
    bell_filter bell;
} audio_filter;

static void filter_lowpass(audio_filter *filter, float *buf, int size)
{
    float filtered[8] = {0};

    for (int ch = 0; ch < filter->nb_channels; ch++)
        filtered[ch] = buf[ch];

    for (int i = filter->nb_channels; i < size; i += filter->nb_channels)
    {
        for (int ch = 0; ch < filter->nb_channels; ch++)
        {
            buf[i + ch] =
                filtered[ch] + (filter->alpha * (buf[i + ch] - filtered[ch]));
            filtered[ch] = buf[i + ch];
        }
    }
}

static void filter_highpass(audio_filter *filter, float *buf, int size)
{
    float filtered[8] = {0};
    float prev_sample[8] = {0};
    float sample[8] = {0};

    for (int ch = 0; ch < filter->nb_channels; ch++)
    {
        filtered[ch] = buf[ch];
        prev_sample[ch] = buf[ch];
    }

    for (int i = filter->nb_channels; i < size; i += filter->nb_channels)
    {
        for (int ch = 0; ch < filter->nb_channels; ch++)
        {
            sample[ch] = buf[i + ch];
            buf[i + ch] = filter->alpha * filtered[ch] +
                          filter->alpha * (sample[ch] - prev_sample[ch]);
            filtered[ch] = buf[i + ch];
            prev_sample[ch] = sample[ch];
        }
    }
}

static void _filter_bell(audio_filter *filter, float *buf, int size)
{
    bell_filter bell = filter->bell;

    for (int i = 0; i < size; i += filter->nb_channels)
    {
        for (int ch = 0; ch < filter->nb_channels; ch++)
        {
            float sample = buf[i + ch];
            float sample_out = bell.b0 * sample + bell.b1 * bell.x1 +
                               bell.b2 * bell.x2 - bell.a1 * bell.y1 -
                               bell.a2 * bell.y2;
            bell.x2 = bell.x1;
            bell.x1 = sample;
            bell.y2 = bell.y1;
            bell.y1 = sample_out;

            buf[i + ch] = sample_out;
        }
    }
}

static void update_param(audio_filter *filter, filter_param *param)
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

void eff_filter_proccess(audio_effect *eff, float *buf, int size)
{
    audio_filter *ctx = eff->ctx;
    enum audio_filt_type type = ctx->type;
    if (type == AUDIO_FILT_LOWPASS)
        filter_lowpass(eff->ctx, buf, size);
    else if (type == AUDIO_FILT_HIGHPASS)
        filter_highpass(eff->ctx, buf, size);
    else if (type == AUDIO_FILT_BELL)
        _filter_bell(eff->ctx, buf, size);
    else
        fprintf(stderr, "Unknown filter type: %d\n", type);
}

void eff_filter_free(audio_effect *eff)
{
    if (!eff)
        return;

    if (eff->ctx != NULL)
        free(eff->ctx);
    eff->ctx = NULL;
}

audio_effect audio_eff_filter(enum audio_filt_type type, float freq,
                              int nb_channels, int sample_rate,
                              filter_param *param)
{
    audio_effect eff = {0};

    eff.process = eff_filter_proccess;
    eff.free = eff_filter_free;
    eff.type = AUDIO_EFF_FILTER;
    eff.ctx = calloc(1, sizeof(audio_filter));
    if (!eff.ctx)
    {
        errno = -ENOMEM;
        goto exit;
    }

    audio_filter *ctx = eff.ctx;
    ctx->type = type;
    ctx->freq = freq;
    ctx->nb_channels = nb_channels;
    ctx->sample_rate = sample_rate;

    update_param(ctx, param);

exit:
    return eff;
}

void audio_eff_filter_set(audio_effect *eff, enum audio_filt_type type,
                          float freq, int nb_channels, int sample_rate,
                          filter_param *param)
{
    audio_filter *ctx = eff->ctx;
    ctx->type = type;
    ctx->freq = freq;
    ctx->nb_channels = nb_channels;
    ctx->sample_rate = sample_rate;

    update_param(ctx, param);
}
