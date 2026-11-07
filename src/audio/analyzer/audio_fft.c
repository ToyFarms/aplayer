#ifndef _USE_MATH_DEFINES
#  define _USE_MATH_DEFINES /* M_PI on MSVC */
#endif

#include "audio_analyzer.h"
#include "fftw3.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * A sliding window over the last ANALYZER_FFT_FREQ_SIZE samples of the mono
 * mix. Every call shifts the new samples in, applies a Hann window (without
 * one a tone smears over the whole spectrum) and transforms. The result is an
 * amplitude: a full-scale sine reads 1.0 at its bin.
 */

typedef struct analyzer_fft
{
    fftw_plan plan;
    double *in;  /* the last `in_size` samples, newest last */
    double *win; /* `in` times the window: what the plan reads */
    double *window;
    int in_size;
    fftw_complex *out;

    int out_size;
    float *freqs;
    float norm; /* 1 / (sum of the window / 2): a sine's amplitude from |X| */
} analyzer_fft;

void fft_free(audio_analyzer *analyzer)
{
    analyzer_fft *ctx = analyzer->ctx;

    fftw_destroy_plan(ctx->plan);
    fftw_free(ctx->in);
    fftw_free(ctx->win);
    fftw_free(ctx->out);
    free(ctx->window);
    free(ctx->freqs);
    free(ctx);

    analyzer->ctx = NULL;
}

void fft_process(audio_analyzer *analyzer, audio_callback_param p)
{
    analyzer_fft *ctx = analyzer->ctx;
    if (p.nb_channels <= 0 || p.sample_rate <= 0)
        return;

    int nb_samples = p.size / p.nb_channels;
    if (nb_samples <= 0)
        return;
    /* a block longer than the window: only its end matters */
    int skip = nb_samples > ctx->in_size ? nb_samples - ctx->in_size : 0;
    int n = nb_samples - skip;

    memmove(ctx->in, ctx->in + n, (ctx->in_size - n) * sizeof(ctx->in[0]));
    for (int i = 0; i < n; i++)
    {
        const float *frame = p.out + (size_t)(skip + i) * p.nb_channels;
        double m = 0.0;
        for (int ch = 0; ch < p.nb_channels; ch++)
            m += frame[ch];
        ctx->in[ctx->in_size - n + i] = m / p.nb_channels;
    }

    for (int i = 0; i < ctx->in_size; i++)
        ctx->win[i] = ctx->in[i] * ctx->window[i];
    fftw_execute(ctx->plan);

    for (int i = 0; i < ctx->out_size; i++)
        ctx->freqs[i] = ctx->norm * sqrtf((float)(ctx->out[i][0] * ctx->out[i][0] +
                                                  ctx->out[i][1] * ctx->out[i][1]));

    analyzer->callback(&(analyzer_fft_ctx){.freqs = ctx->freqs,
                                           .size = ctx->out_size,
                                           .sample_rate = p.sample_rate},
                       analyzer->userdata);
}

audio_analyzer audio_analyzer_fft(analyzer_callback callback, void *userdata)
{
    audio_analyzer analyzer = {0};

    analyzer.type = AUDIO_ANALYZER_FFT;
    analyzer.callback = callback;
    analyzer.userdata = userdata;
    analyzer.free = fft_free;
    analyzer.process = fft_process;
    analyzer.ctx = calloc(1, sizeof(analyzer_fft));
    assert(analyzer.ctx != NULL);

#define IS_POWER_OF_TWO(x) (((x) & ((x) - 1)) == 0)
    assert(IS_POWER_OF_TWO(ANALYZER_FFT_FREQ_SIZE));

    analyzer_fft *ctx = analyzer.ctx;
    ctx->in_size = ANALYZER_FFT_FREQ_SIZE;
    ctx->out_size = ANALYZER_FFT_FREQ_SIZE / 2 + 1;

    ctx->in = fftw_malloc(ctx->in_size * sizeof(double));
    assert(ctx->in != NULL);
    memset(ctx->in, 0, ctx->in_size * sizeof(double));

    ctx->win = fftw_malloc(ctx->in_size * sizeof(double));
    assert(ctx->win != NULL);
    memset(ctx->win, 0, ctx->in_size * sizeof(double));

    ctx->out = fftw_malloc(ctx->out_size * sizeof(fftw_complex));
    assert(ctx->out != NULL);

    ctx->freqs = malloc(ctx->out_size * sizeof(float));
    assert(ctx->freqs != NULL);

    ctx->window = malloc(ctx->in_size * sizeof(double));
    assert(ctx->window != NULL);
    double sum = 0.0;
    for (int i = 0; i < ctx->in_size; i++)
    {
        /* periodic Hann: exact for a sliding transform */
        ctx->window[i] =
            0.5 - 0.5 * cos(2.0 * M_PI * (double)i / (double)ctx->in_size);
        sum += ctx->window[i];
    }
    ctx->norm = (float)(2.0 / sum);

    ctx->plan =
        fftw_plan_dft_r2c_1d(ctx->in_size, ctx->win, ctx->out, FFTW_ESTIMATE);

    return analyzer;
}
