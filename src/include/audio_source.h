#ifndef __AUDIO_SOURCE_H
#define __AUDIO_SOURCE_H

#include "array.h"
#include "metagen.h"
#include "ring_buf.h"
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

enum audio_err
{
    AUDIO_CONTINUE = 0,
    AUDIO_EOF,
    AUDIO_NO_DATA,
    AUDIO_FULL,
    AUDIO_ERROR,
};

typedef struct audio_chunk_marker
{
    int nb_frames;
    int64_t timestamp;
} audio_chunk_marker;

typedef struct audio_source
{
    void *ctx;
    // TODO: normalize time unit
    enum audio_err (*update)(struct audio_source *);
    void (*free)(struct audio_source *);

    enum audio_err (*get_frame)(struct audio_source *, int req_sample,
                                void *out, int *out_samples);
    void (*seek)(struct audio_source *, int64_t ms, int whence);

    // optional, callee needs to check if its implemented before calling it
    void (*get_arts)(struct audio_source *, array(image_t) * out);
    metadata_t *(*get_metadata)(struct audio_source *);
    float (*get_loudness)(struct audio_source *);

    uint64_t id;

    int nb_channels;
    int sample_rate;
    enum AVSampleFormat sample_fmt;

    array(audio_effect) effects;
    array(audio_analyzer) analyzer;

    // is source realtime (e.g. microphone source)
    bool is_realtime;
    // true if decoding reached eof or stream is finished
    bool is_eof;
    // true if source is finished (eof && buffer empty)
    bool is_finished;

    // this is in microsecond (AV_TIME_BASE)
    int64_t timestamp;
    int64_t duration;

    array(ring_buf_t) planes;
    array(audio_chunk_marker) markers;

    pthread_mutex_t ctx_mutex;
} audio_source;

int audio_common_init(audio_source *audio);
void audio_common_free(audio_source *audio);
int audio_common_push_frame(audio_source *audio, AVFrame *frame,
                            int64_t timestamp_us);
enum audio_err audio_common_get_frame(audio_source *audio, int req_sample,
                                      void *out, int *out_samples);

audio_source audio_from_file(const char *filename);

#endif /* __AUDIO_SOURCE_H */
