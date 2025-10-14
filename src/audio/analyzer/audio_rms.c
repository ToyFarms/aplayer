#include "audio_analyzer.h"

#include <ebur128.h>
#include <float.h>
#include <math.h>

void rms_free(audio_analyzer *analyzer)
{
    (void)analyzer;
}

void rms_process(audio_analyzer *analyzer, audio_callback_param p)
{
    float rms[8] = {0};

    for (int i = 0; i < p.size; i += p.nb_channels)
    {
        for (int ch = 0; ch < p.nb_channels; ch++)
        {
            float s = p.out[i + ch];
            rms[ch] += s * s;
        }
    }

    float nb_samples = (float)p.size / (float)p.nb_channels;
    for (int ch = 0; ch < p.nb_channels; ch++)
        rms[ch] = sqrtf(rms[ch] / nb_samples);

    analyzer->callback(
        &(analyzer_rms_ctx){.rms = rms, .nb_channels = p.nb_channels},
        analyzer->userdata);
}

audio_analyzer audio_analyzer_rms(analyzer_callback callback, void *userdata)
{
    audio_analyzer analyzer = {0};

    analyzer.callback = callback;
    analyzer.userdata = userdata;
    analyzer.free = rms_free;
    analyzer.process = rms_process;

    return analyzer;
}
