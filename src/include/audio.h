#ifndef __AUDIO_H
#define __AUDIO_H

#include "array.h"
#include "audio_format.h"

#include <stdint.h>

typedef struct audio_src
{
    void *ctx;
    int (*update)(struct audio_src *);
    void (*free)(struct audio_src *);
    int (*get_frame)(struct audio_src *, int req_sample, float *out);

    int stream_nb_channels;
    int stream_sample_rate;
    enum AVSampleFormat stream_sample_fmt;
    int target_nb_channels;
    int target_sample_rate;
    enum AVSampleFormat target_sample_fmt;

    array(audio_effect) effects;

    // is source realtime (e.g. microphone source)
    bool is_realtime;
    // true if decoding reached eof (not relevant for realtime source)
    bool is_eof;
    // true if source is finished (eof && buffer empty)
    bool is_finished;
} audio_src;

#define AUDIOSRC_IDX(arr, index) (((audio_src *)(arr).data)[index])

int audio_common_init(audio_src *audio);
void audio_common_free(audio_src *audio);

int audio_set_metadata(audio_src *audio, int nb_channels, int sample_rate,
                       enum audio_format sample_fmt);
audio_src audio_from_file(const char *filename, int nb_channels,
                          int sample_rate, enum audio_format sample_fmt);

#endif /* __AUDIO_H */
