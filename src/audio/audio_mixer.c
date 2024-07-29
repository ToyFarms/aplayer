#include "audio_mixer.h"
#include "audio.h"
#include "audio_analyzer.h"
#include "audio_effect.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

audio_mixer mixer_create(int nb_channels, int sample_rate,
                         enum audio_format sample_fmt)
{
    assert(AUDIO_IS_PLANAR(sample_fmt) == false && sample_fmt == AUDIO_FLT);

    audio_mixer mixer = {0};

    mixer.nb_channels = nb_channels;
    mixer.sample_rate = sample_rate;
    mixer.sample_fmt = sample_fmt;

    mixer.sources = array_create(16, sizeof(audio_src));
    if (errno != 0)
        fprintf(stderr, "Cannot allocate mixer sources\n");

    mixer.master_effect = array_create(16, sizeof(audio_effect));
    if (errno != 0)
        fprintf(stderr, "Cannot allocate master effect\n");

    mixer.master_analyzer = array_create(16, sizeof(audio_analyzer));
    if (errno != 0)
        fprintf(stderr, "Cannot allocate master analyzer\n");

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

    for (int i = 0; i < mixer->master_effect.length; i++)
    {
        audio_effect eff = AUDIOEFF_IDX(mixer->master_effect, i);
        eff.free(&eff);
    }
    array_free(&mixer->master_effect);

    for (int i = 0; i < mixer->master_analyzer.length; i++)
    {
        audio_analyzer analyzer = AUDIOANALYZER_IDX(mixer->master_analyzer, i);
        analyzer.free(&analyzer);
    }
    array_free(&mixer->master_analyzer);
}

int mixer_get_frame(audio_mixer *mixer, float *out)
{
    float src_buf[48000] = {0};
    int frame_size = 1024;
    int ret;
    int len;
    bool finished = true;

    for (int i = 0; i < mixer->sources.length; i++)
    {
        audio_src *src = &AUDIOSRC_IDX(mixer->sources, i);
        if (src->is_finished)
            continue;

        ret = src->update(src);
        if (ret != EOF && ret < 0)
            continue;

        ret = len = src->get_frame(src, frame_size, src_buf);
        if (ret == -ENODATA)
        {
            fprintf(stderr,
                    "Frame size too big, not enough data in the "
                    "buffer. Call update() again or lower the frame size\n");
            continue;
        }
        else if (ret == EOF)
        {
            src->is_finished = true;
            ret = len = src->get_frame(src, -1, src_buf);
        }

        for (int j = 0; j < src->effects.length; j++)
        {
            audio_effect eff = AUDIOEFF_IDX(src->effects, j);
            eff.process(&eff, src_buf, len, src->target_nb_channels);
        }

        for (int sample = 0; sample < len; sample++)
            out[sample] += src_buf[sample];

        finished = false;
    }

    for (int i = 0; i < mixer->master_effect.length; i++)
    {
        audio_effect eff = AUDIOEFF_IDX(mixer->master_effect, i);
        eff.process(&eff, out, frame_size, mixer->nb_channels);
    }

    for (int i = 0; i < mixer->master_analyzer.length; i++)
    {
        audio_analyzer analyzer = AUDIOANALYZER_IDX(mixer->master_analyzer, i);
        analyzer.process(&analyzer, out, frame_size, mixer->nb_channels);
    }

    if (finished)
        return EOF;

    return frame_size;
}
