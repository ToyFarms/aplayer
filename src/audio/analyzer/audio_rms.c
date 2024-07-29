#include "audio_analyzer.h"

#include <math.h>

void analyzer_free(audio_analyzer *analyzer)
{
    (void)analyzer;
}

void analyzer_process(audio_analyzer *analyzer, float *buf, int size,
                      int nb_channels)
{
    float rms[8] = {0};

    for (int i = 0; i < size; i += nb_channels)
    {
        for (int ch = 0; ch < nb_channels; ch++)
            rms[ch] += buf[i + ch] * buf[i + ch];
    }

    float nb_samples = (float)size / (float)nb_channels;
    for (int ch = 0; ch < nb_channels; ch++)
        rms[ch] = sqrtf(rms[ch] / nb_samples);

    analyzer->callback(
        &(analyzer_rms_ctx){.rms = rms, .nb_channels = nb_channels},
        analyzer->userdata);
}

audio_analyzer audio_analyzer_rms(analyzer_callback callback, void *userdata)
{
    audio_analyzer analyzer = {0};

    analyzer.type = ANALYZER_RMS;
    analyzer.callback = callback;
    analyzer.userdata = userdata;
    analyzer.free = analyzer_free;
    analyzer.process = analyzer_process;

    return analyzer;
}
