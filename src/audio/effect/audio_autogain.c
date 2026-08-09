#include "audio_effect.h"
#include <assert.h>
#include <ebur128.h>
#include <errno.h>
#include <math.h>
#include <string.h>

static const float TARGET_LUFS = -18.0;

static void autogain_free(audio_effect *eff)
{
    _audio_eff_free_default(eff);
}

static void autogain_process(audio_effect *eff, audio_callback_param p)
{
    if (p.src == NULL)
        return;
    if (p.src->get_loudness == NULL)
        return;

    double loudness = p.src->get_loudness(p.src);
    if (isnan(loudness) || isinf(loudness))
        return;

    for (int i = 0; i < p.size; i++)
        p.out[i] *= powf(10.0f, (TARGET_LUFS - loudness) / 20.0f);
}

audio_effect audio_eff_autogain()
{
    errno = 0;
    audio_effect eff = {0};

    eff.process = autogain_process;
    eff.free = autogain_free;
    eff.type = AUDIO_EFF_AUTOGAIN;

    return eff;
}
