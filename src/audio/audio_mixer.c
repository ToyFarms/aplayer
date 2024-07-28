#include "audio_mixer.h"
#include "audio.h"
#include "audio_effect.h"

#include <errno.h>
#include <stdio.h>

audio_mixer mixer_create()
{
    audio_mixer mixer = {0};

    mixer.sources = array_create(16, sizeof(audio_src));
    if (errno != 0)
        fprintf(stderr, "Cannot allocate mixer sources\n");

    return mixer;
}

void mixer_free(audio_mixer *mixer)
{
    if (mixer == NULL)
        return;

    for (int i = 0; i < mixer->sources.length; i++)
    {
        audio_src src = AUDIOSRC_IDX(mixer->sources, i);
        for (int j = 0; j < src.effects.length; j++)
        {
            audio_effect eff = AUDIOEFF_IDX(src.effects, j);
            eff.free(&eff);
        }
        src.free(&src);
    }

    array_free(&mixer->sources);
}

int mixer_get_frame(audio_mixer *mixer, float *out)
{
    float src_buf[48000] = {0};
    int frame_size = 1024;
    int ret;
    int len;

    for (int i = 0; i < mixer->sources.length; i++)
    {
        audio_src src = AUDIOSRC_IDX(mixer->sources, i);
        ret = src.update(&src);
        if (ret < 0)
            continue;
        ret = len = src.get_frame(&src, frame_size, src_buf);
        if (ret < 0)
            continue;

        for (int j = 0; j < src.effects.length; j++)
        {
            audio_effect eff = AUDIOEFF_IDX(src.effects, j);
            eff.process(&eff, src_buf, len);
        }

        for (int sample = 0; sample < len; sample++)
            out[sample] += src_buf[sample];
    }

    return frame_size;
}
