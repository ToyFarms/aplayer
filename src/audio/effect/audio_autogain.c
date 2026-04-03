#include "audio_effect.h"
#include <assert.h>
#include <ebur128.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct effect_autogain
{
    audio_source *src;
    float current_gain;

    pthread_t tid;
} effect_autogain;

static void autogain_free(audio_effect *eff)
{
    effect_autogain *ctx = eff->ctx;

    ctx->src->free(ctx->src);
    _audio_eff_free_default(eff);
}

static void autogain_process(audio_effect *eff, audio_callback_param p)
{
    effect_autogain *ctx = eff->ctx;

    float gain = powf(10.0f, ctx->current_gain / 20.0f);
    for (int i = 0; i < p.size; i++)
        p.out[i] *= gain;
}

audio_effect audio_eff_autogain()
{
    errno = 0;
    audio_effect eff = {0};

    eff.process = autogain_process;
    eff.free = autogain_free;
    eff.type = AUDIO_EFF_AUTOGAIN;
    eff.ctx = calloc(1, sizeof(effect_autogain));
    assert(eff.ctx != NULL);

    return eff;
}

static void *_compute(void *arg)
{
    audio_effect *eff = arg;
    effect_autogain *ctx = eff->ctx;

    audio_source *src = ctx->src;

    ebur128_state *st = ebur128_init(src->target_nb_channels,
                                     src->target_sample_rate, EBUR128_MODE_I);
    if (st == NULL)
        return NULL;

    ebur128_set_max_window(st, 400.0f);

    int ret = 0, len = 0;
    int req_sample = src->target_sample_rate * 0.1;
    float *buf = calloc(req_sample, sizeof(*buf));
    float target_lufs = -18.0;

    int current_samples = 0;

    while (!src->is_finished)
    {
        while (!src->is_eof && src->buffer.length < req_sample)
            ret = src->update(src);

        ret = len = src->get_frame(src, req_sample, buf);
        if (ret == -ENODATA)
            continue;
        else if (ret == EOF)
        {
            src->is_finished = true;
            ret = len = src->get_frame(src, -1, buf);
            src->free(src);
        }

        ebur128_add_frames_float(st, buf, len / src->target_nb_channels);

        double measured_lufs = 0.0;
        int ret = ebur128_loudness_global(st, &measured_lufs);

        ctx->current_gain = target_lufs - measured_lufs;
    }

    ebur128_destroy(&st);
    return NULL;
}

void audio_eff_autogain_set(audio_effect *eff, audio_source *_src)
{
    audio_source *src = malloc(sizeof(*src));
    memcpy(src, _src, sizeof(*src));

    if (src->is_realtime)
    {
        errno = -EINVAL;
        return;
    }

    effect_autogain *ctx = eff->ctx;
    if (ctx->src != NULL)
        ctx->src->free(ctx->src);
    ctx->src = src;

    pthread_create(&ctx->tid, NULL, _compute, eff);
}
