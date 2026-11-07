#ifndef __AUDIO_RESAMPLER_H
#define __AUDIO_RESAMPLER_H

#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"

#include <stdint.h>

typedef struct resample_ctx
{
    SwrContext *swr;
    AVFrame *scratch_frame;

    int src_nb_channels;
    int src_sample_rate;
    enum AVSampleFormat src_sample_fmt;

    int tgt_nb_channels;
    int tgt_sample_rate;
    enum AVSampleFormat tgt_sample_fmt;
} resample_ctx_t;

typedef int (*resample_sink_fn)(void *sink_ctx, AVFrame *frame);

int resample_ctx_init(resample_ctx_t *rs);
void resample_ctx_free(resample_ctx_t *rs);

int resample_convert(resample_ctx_t *rs, const uint8_t **src_data,
                     int src_nb_samples, enum AVSampleFormat src_fmt,
                     int src_ch, int src_sample_rate,
                     enum AVSampleFormat tgt_fmt, int tgt_ch,
                     int tgt_sample_rate, resample_sink_fn sink,
                     void *sink_ctx);

#endif /* __AUDIO_RESAMPLER_H */
