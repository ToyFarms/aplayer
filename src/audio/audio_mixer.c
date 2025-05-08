#include "audio_mixer.h"
#include "audio_effect.h"
#include "audio_source.h"
#include "exception.h"
#include "logger.h"
#include <omp.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

audio_mixer mixer_create(int nb_channels, int sample_rate,
                         enum audio_format sample_fmt)
{
    // TODO: add support for planar audio, idk about non float sample format
    assert(AUDIO_IS_PLANAR(sample_fmt) == false && sample_fmt == AUDIO_FLT);

    audio_mixer mixer = {0};

    mixer.nb_channels = nb_channels;
    mixer.sample_rate = sample_rate;
    mixer.sample_fmt = sample_fmt;
    pthread_mutex_init(&mixer.source_mutex, NULL);

    mixer.sources = array_create(16, sizeof(audio_source));
    if (errno != 0)
        log_error("Cannot allocate mixer sources: %s\n", strerror(errno));

    return mixer;
}

void mixer_free(audio_mixer *mixer)
{
    if (mixer == NULL)
        return;

    pthread_mutex_lock(&mixer->source_mutex);
    audio_source src;
    ARR_FOREACH(mixer->sources, src, i)
    {
        audio_effect eff;
        ARR_FOREACH(src.pipeline, eff, j)
        {
            eff.free(&eff);
        }
        src.free(&src);
    }
    array_free(&mixer->sources);
    pthread_mutex_unlock(&mixer->source_mutex);
    pthread_mutex_destroy(&mixer->source_mutex);
}

int mixer_get_frame(audio_mixer *mixer, int req_sample, float *out)
{
    float src_buf[req_sample];
    memset(src_buf, 0, req_sample * sizeof(*src_buf));

    int ret;
    int len;
    bool finished = mixer->sources.length != 0;

    pthread_mutex_lock(&mixer->source_mutex);
    audio_source *src;
    ARR_FOREACH_BYREF(mixer->sources, src, i)
    {
        if (src->is_finished)
            continue;

    update:
        ret = src->update(src);
        if (ret != EOF && ret < 0)
            continue;

        ret = len = src->get_frame(src, req_sample, src_buf);
        if (ret == -ENODATA)
            goto update;
        else if (ret == EOF)
        {
            src->is_finished = true;
            ret = len = src->get_frame(src, -1, src_buf);
            src->free(src);
            array_remove(&mixer->sources, i, 1);
        }

        if (i == 0)
            memcpy(out, src_buf, len * sizeof(float));
        else
            for (int sample = 0; sample < len; sample++)
                out[sample] += src_buf[sample];

        finished = false;
    }
    pthread_mutex_unlock(&mixer->source_mutex);

    if (finished)
        return EOF;

    return 0;
}
