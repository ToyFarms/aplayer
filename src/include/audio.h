#ifndef __AUDIO_H
#define __AUDIO_H

#include "audio_format.h"

#include <stdint.h>

typedef struct audio_src
{
    void *ctx;
    int (*update)(struct audio_src *);
    void (*free)(struct audio_src *);
    int (*get_frame)(struct audio_src *, int, float *);

    int stream_nb_channels;
    int stream_sample_rate;
    enum AVSampleFormat stream_sample_fmt;
    int target_nb_channels;
    int target_sample_rate;
    enum AVSampleFormat target_sample_fmt;

    bool is_realtime;
    bool is_finished;
} audio_src;

int audio_set_metadata(audio_src *audio, int nb_channels, int sample_rate,
                       enum audio_format sample_fmt);
audio_src audio_from_file(const char *filename);

#endif /* __AUDIO_H */
