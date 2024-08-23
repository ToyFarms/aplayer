#ifndef __AUDIO_ANALYZER_H
#define __AUDIO_ANALYZER_H

#include "audio_callback.h"

#include <stdbool.h>

struct audio_analyzer;
typedef void (*analyzer_callback)(void *actx, void *userdata);

enum audio_analyzer_type
{
    AUDIO_ANALYZER_RMS,
    AUDIO_ANALYZER_RMS_HUMAN,
    AUDIO_ANALYZER_FFT,
};

typedef struct audio_analyzer
{
    void *ctx;
    void *userdata;
    void (*free)(struct audio_analyzer *);
    void (*process)(struct audio_analyzer *, audio_callback_param);
    analyzer_callback callback;

    enum audio_analyzer_type type;
} audio_analyzer;

#define AUDIOANALYZER_IDX(arr, index) (((audio_analyzer *)(arr.data))[index])

typedef struct analyzer_rms_ctx
{
    float *rms;
    int nb_channels;
} analyzer_rms_ctx;

// adjust_human: apply K-weighting to closely match human hearing
audio_analyzer audio_analyzer_rms(bool adjust_human, analyzer_callback callback,
                                  void *userdata);

#define ANALYZER_FFT_FREQ_SIZE (1 << 13)

typedef struct analyzer_fft_ctx
{
    float *freqs;
    int size;
    int sample_rate;
} analyzer_fft_ctx;

audio_analyzer audio_analyzer_fft(analyzer_callback callback, void *userdata);

#endif /* __AUDIO_ANALYZER_H */
