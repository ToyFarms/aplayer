#include "audio_effect.h"
#include "clock.h"
#include "logger.h"
#include "ring_buf.h"
#include <assert.h>
#include <ebur128.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct effect_autogain
{
    float current_gain;
    ebur128_state *st;
} effect_autogain;

static const float TARGET_LUFS = -18.0;

static void autogain_free(audio_effect *eff)
{
    effect_autogain *ctx = eff->ctx;
    if (ctx->st)
        ebur128_destroy(&ctx->st);

    _audio_eff_free_default(eff);
}

static void audio_eff_autogain_integrate(audio_effect *eff, float *samples,
                                         int len, int nb_channels)
{
    effect_autogain *ctx = eff->ctx;
    if (!ctx->st)
        return;

    ebur128_add_frames_float(ctx->st, samples, len / nb_channels);

    double measured_lufs = 0.0;
    int ret = ebur128_loudness_global(ctx->st, &measured_lufs);
    if (!isnan(measured_lufs) && !isinf(measured_lufs))
        ctx->current_gain = powf(10.0f, (TARGET_LUFS - measured_lufs) / 20.0f);
    else
        log_warning("autogain integrate failed: measured lufs is %f\n",
                    measured_lufs);
}

static void autogain_process(audio_effect *eff, audio_callback_param p)
{
    effect_autogain *ctx = eff->ctx;

    audio_eff_autogain_integrate(eff, p.out, p.size, p.nb_channels);

    for (int i = 0; i < p.size; i++)
        p.out[i] *= ctx->current_gain;
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

float audio_eff_autogain_get_gain(audio_effect *eff)
{
    effect_autogain *ctx = eff->ctx;
    return ctx->current_gain;
}

void audio_eff_autogain_initial(audio_effect *eff, audio_source *src)
{
    if (src->is_realtime)
    {
        errno = -EINVAL;
        return;
    }

    effect_autogain *ctx = eff->ctx;

    if (ctx->st != NULL)
        ebur128_destroy(&ctx->st);

    ctx->st = ebur128_init(src->target_nb_channels, src->target_sample_rate,
                           EBUR128_MODE_I);
    if (ctx->st == NULL)
        return;

    ebur128_set_max_window(ctx->st, 400.0f);

    int ret = 0, len = 0;

    const float probe_seconds = 2.0f;
    const int probe_count = 20;

    int req_sample = (int)(src->target_sample_rate * src->target_nb_channels *
                           probe_seconds);
    float *buf = calloc(req_sample, sizeof(*buf));
    if (buf == NULL)
        return;

    int64_t saved_ts = src->timestamp;
    src->seek(src, 0, SEEK_SET);

    int64_t stride = src->duration / probe_count;

    for (int i = 0; i < probe_count; i++)
    {
        while (!src->is_eof && src->buffer.length < req_sample)
            ret = src->update(src);

        ret = len = src->get_frame(src, req_sample, buf);
        if (ret == -ENODATA)
        {
            src->seek(src, US2MS(stride), SEEK_CUR);
            continue;
        }
        else if (ret == EOF)
        {
            ret = len = src->get_frame(src, -1, buf);
            if (ret > 0)
                ebur128_add_frames_float(ctx->st, buf,
                                         len / src->target_nb_channels);
            break;
        }

        ebur128_add_frames_float(ctx->st, buf, len / src->target_nb_channels);

        src->seek(src, US2MS(stride), SEEK_CUR);
    }

    free(buf);

    double measured_lufs = 0.0;
    ret = ebur128_loudness_global(ctx->st, &measured_lufs);
    if (!isnan(measured_lufs) && !isinf(measured_lufs))
        ctx->current_gain = powf(10.0f, (TARGET_LUFS - measured_lufs) / 20.0f);
    else
        log_warning("autogain failed: measured lufs is %f\n", measured_lufs);

    log_debug(
        "autogain: initial measured lufs %f, gain %f dbfs (target %f dbfs)\n",
        measured_lufs, TARGET_LUFS - measured_lufs, TARGET_LUFS);

    src->seek(src, US2MS(saved_ts), SEEK_SET);
}
