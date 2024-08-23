#include "audio_analyzer.h"
#include "fftw3.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct analyzer_fft
{
    fftw_plan plan;
    double *in;
    int in_size;
    int in_len;
    fftw_complex *out;
    int out_size;
} analyzer_fft;

void fft_free(audio_analyzer *analyzer)
{
    analyzer_fft *ctx = analyzer->ctx;

    fftw_destroy_plan(ctx->plan);
    fftw_free(ctx->in);
    fftw_free(ctx->out);

    free(ctx);
    analyzer->ctx = NULL;
}

void fft_process(audio_analyzer *analyzer, audio_callback_param p)
{
    analyzer_fft *ctx = analyzer->ctx;

    fftw_execute(ctx->plan);

    int nb_samples = p.size / p.nb_channels;
    memmove(ctx->in, ctx->in + nb_samples, (ctx->in_size - nb_samples) * sizeof(ctx->in[0]));
    for (int i = 0; i < nb_samples; i++)
        ctx->in[ctx->in_size - i] = p.out[(nb_samples - i) * p.nb_channels];

    float freqs[ctx->out_size];
    for (int i = 0; i < ctx->out_size; i++)
        freqs[i] = sqrtf(ctx->out[i][0] * ctx->out[i][0] +
                         ctx->out[i][1] * ctx->out[i][1]);

    analyzer->callback(&(analyzer_fft_ctx){.freqs = freqs,
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

#define IS_POWER_OF_TWO(x) (x & (x - 1)) == 0
    assert(IS_POWER_OF_TWO(ANALYZER_FFT_FREQ_SIZE));

    analyzer_fft *ctx = analyzer.ctx;
    ctx->in_size = ANALYZER_FFT_FREQ_SIZE;
    ctx->out_size = ANALYZER_FFT_FREQ_SIZE / 2 + 1;
    ctx->in = fftw_malloc(ctx->in_size * sizeof(double));
    assert(ctx->in != NULL);
    ctx->out = fftw_malloc(ctx->out_size * sizeof(fftw_complex));
    assert(ctx->out != NULL);

    ctx->plan = fftw_plan_dft_r2c_1d(ctx->in_size, ctx->in, ctx->out, FFTW_ESTIMATE);

    return analyzer;
}
