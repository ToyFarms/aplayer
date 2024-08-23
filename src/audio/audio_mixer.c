#include "audio_mixer.h"
#include "audio_effect.h"
#include "audio_source.h"
#include "exception.h"
#include "logger.h"
#include "plugin.h"
#include <omp.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

audio_mixer mixer_create(int nb_channels, int sample_rate,
                         enum audio_format sample_fmt)
{
    assert(AUDIO_IS_PLANAR(sample_fmt) == false && sample_fmt == AUDIO_FLT);

    audio_mixer mixer = {0};

    mixer.nb_channels = nb_channels;
    mixer.sample_rate = sample_rate;
    mixer.sample_fmt = sample_fmt;
    pthread_mutex_init(&mixer.source_mutex, NULL);

    mixer.sources = array_create(16, sizeof(audio_source));
    if (errno != 0)
        log_error("Cannot allocate mixer sources: %s\n", strerror(errno));

    mixer.master_plugins = array_create(16, sizeof(plugin_module));
    if (errno != 0)
        log_error("Cannot allocate mixer sources: %s\n", strerror(errno));

    return mixer;
}

void mixer_free(audio_mixer *mixer)
{
    if (mixer == NULL)
        return;

    pthread_mutex_lock(&mixer->source_mutex);
    for (int i = 0; i < mixer->sources.length; i++)
    {
        audio_source src = AUDIOSRC_IDX(mixer->sources, i);
        for (int j = 0; j < src.effects.length; j++)
        {
            audio_effect eff = AUDIOEFF_IDX(src.effects, j);
            eff.free(&eff);
        }
        src.free(&src);
    }
    array_free(&mixer->sources);
    pthread_mutex_unlock(&mixer->source_mutex);

    for (int i = 0; i < mixer->master_plugins.length; i++)
    {
        plugin_module plugin = PLUGIN_IDX(mixer->master_plugins, i);
        plugin.unload(plugin.ctx);
        plugin.ctx = NULL;
    }
    array_free(&mixer->master_plugins);

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
    for (int i = 0; i < mixer->sources.length; i++)
    {
        audio_source *src = &AUDIOSRC_IDX(mixer->sources, i);
        if (src->is_finished)
            continue;

        ret = src->update(src);
        if (ret != EOF && ret < 0)
            continue;

        ret = len = src->get_frame(src, req_sample, src_buf);
        if (ret == -ENODATA)
        {
            log_error("Frame size too big, not enough data in the "
                      "buffer. Call update() again or lower the frame size\n");
            continue;
        }
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

    for (int i = 0; i < mixer->master_plugins.length; i++)
    {
        plugin_module plugin = PLUGIN_IDX(mixer->master_plugins, i);

        plugin_process_param p = {.samples = out,
                                  .size = req_sample,
                                  .nb_channels = mixer->nb_channels,
                                  .sample_rate = mixer->sample_rate};
        PLUGIN_SAFECALL(&plugin, plugin.process(plugin.ctx, p),
                        plugin.unload(plugin.ctx), &mixer->master_plugins, i);
    }

    if (finished)
        return EOF;

    return 0;
}
