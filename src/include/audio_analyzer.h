#ifndef __AUDIO_ANALYZER_H
#define __AUDIO_ANALYZER_H

struct audio_analyzer;
typedef void (*analyzer_callback)(void *ctx, void *userdata);

enum audio_analyzer_type
{
    ANALYZER_RMS,
};

typedef struct audio_analyzer
{
    void *ctx;
    void *userdata;
    void (*free)(struct audio_analyzer *);
    void (*process)(struct audio_analyzer *, float *buf, int size, int nb_channels);
    analyzer_callback callback;

    enum audio_analyzer_type type;
} audio_analyzer;

#define AUDIOANALYZER_IDX(arr, index) (((audio_analyzer *)(arr.data))[index])

typedef struct analyzer_rms_ctx
{
    float *rms;
    int nb_channels;
} analyzer_rms_ctx;

audio_analyzer audio_analyzer_rms(analyzer_callback callback, void *userdata);

#endif /* __AUDIO_ANALYZER_H */
