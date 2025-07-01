#ifndef __AUDIO_SOURCE_H
#define __AUDIO_SOURCE_H

#include "array.h"
#include "audio_format.h"
#include "ring_buf.h"

#include <stdint.h>

typedef struct audio_source
{
    void *ctx;
    int (*update)(struct audio_source *);
    void (*free)(struct audio_source *);
    int (*get_frame)(struct audio_source *, int req_sample, float *out);
    void (*seek)(struct audio_source *, int64_t pos, int whence);
    void (*get_arts)(struct audio_source *, array(image_t) * out);

    int stream_nb_channels;
    int stream_sample_rate;
    enum AVSampleFormat stream_sample_fmt;
    int target_nb_channels;
    int target_sample_rate;
    enum AVSampleFormat target_sample_fmt;

    array(audio_effect) pipeline;

    // is source realtime (e.g. microphone source)
    bool is_realtime;
    // true if decoding reached eof or stream is finished
    bool is_eof;
    // true if source is finished (eof && buffer empty)
    bool is_finished;

    int64_t timestamp;
    int64_t duration;

    ring_buf_t buffer;
} audio_source;

int audio_common_init(audio_source *audio);
void audio_common_free(audio_source *audio);

int audio_set_info(audio_source *audio, int nb_channels, int sample_rate,
                   enum audio_format sample_fmt);
audio_source audio_from_file(const char *filename, int nb_channels,
                             int sample_rate, enum audio_format sample_fmt);

#endif /* __AUDIO_SOURCE_H */
