#include "audio_analyzer.h"
#include "logger.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void rms_free(audio_analyzer *analyzer)
{
    (void)analyzer;
}

static void lfilter(const float *b, int b_size, const float *a, int a_size,
                    const float *samples, int nb_samples, int nb_channels,
                    float *out)
{
    for (int i = 0; i < nb_samples; i += nb_channels)
    {
        for (int ch = 0; ch < nb_channels; ch++)
        {
            int sample = i + ch;

            out[sample] = 0;
            for (int j = 0; j < b_size && sample - j * nb_channels >= 0; j++)
                out[sample] += samples[sample - j * nb_channels] * b[j];

            for (int j = 1; j < a_size && sample - j * nb_channels >= 0; j++)
                out[sample] -= out[sample - j * nb_channels] * a[j];

            if (a_size > 0 && a[0] != 0.0f)
                out[sample] /= a[0];
        }
    }
}

static const float stage1_coeff_a[] = {
    1.0f,
    -1.69065929318241f,
    0.73248077421585f,
};

static const float stage1_coeff_b[] = {
    1.53512485958697f,
    -2.69169618940638f,
    1.19839281085285f,
};

static const float stage2_coeff_a[] = {
    1.0f,
    -1.99004745483398f,
    0.99007225036621f,
};

static const float stage2_coeff_b[] = {
    1.0f,
    -2.0f,
    1.0f,
};

void rms_process(audio_analyzer *analyzer, audio_callback_param p)
{
    float rms[8] = {0};
    float *buf = NULL;
    float filtered[p.size];
    float filtered2[p.size];

    if (analyzer->type == AUDIO_ANALYZER_RMS)
        buf = p.out;
    else if (analyzer->type == AUDIO_ANALYZER_RMS_HUMAN)
    {
        lfilter(stage1_coeff_b, 3, stage1_coeff_a, 3, p.out, p.size, p.nb_channels,
                filtered);
        lfilter(stage2_coeff_b, 3, stage2_coeff_a, 3, filtered, p.size,
                p.nb_channels, filtered2);
        buf = filtered2;
    }
    else
        log_error("Expected RMS type, got=%d\n", analyzer->type);

    for (int i = 0; i < p.size; i += p.nb_channels)
    {
        for (int ch = 0; ch < p.nb_channels; ch++)
            rms[ch] += buf[i + ch] * buf[i + ch];
    }

    float nb_samples = (float)p.size / (float)p.nb_channels;
    for (int ch = 0; ch < p.nb_channels; ch++)
        rms[ch] = sqrtf(rms[ch] / nb_samples);

    analyzer->callback(
        &(analyzer_rms_ctx){.rms = rms, .nb_channels = p.nb_channels},
        analyzer->userdata);
}

audio_analyzer audio_analyzer_rms(bool adjust_human, analyzer_callback callback,
                                  void *userdata)
{
    audio_analyzer analyzer = {0};

    analyzer.type = adjust_human ? AUDIO_ANALYZER_RMS_HUMAN : AUDIO_ANALYZER_RMS;
    analyzer.callback = callback;
    analyzer.userdata = userdata;
    analyzer.free = rms_free;
    analyzer.process = rms_process;

    return analyzer;
}
