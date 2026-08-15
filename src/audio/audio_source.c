#include "audio_source.h"
#include "array.h"
#include "audio_analyzer.h"
#include "audio_effect.h"

#include <errno.h>
#include <pthread.h>

int audio_common_init(audio_source *audio)
{
    audio->effects = array_create(16, sizeof(audio_effect));
    audio->analyzer = array_create(16, sizeof(audio_analyzer));
    audio->buffer =
        ring_buf_create(audio->target_sample_rate * 30, sizeof(float));
    if (pthread_mutex_init(&audio->ctx_mutex, NULL) != 0)
    {
        log_error("Failed to initialize context mutex\n");
        errno = -ENOMEM;
    }

    return errno;
}

void audio_common_free(audio_source *audio)
{
    audio_effect *eff;
    ARR_FOREACH_BYREF(audio->effects, eff, i)
    {
        eff->free(eff);
    }
    array_free(&audio->effects);
    audio_analyzer *analyzer;
    ARR_FOREACH_BYREF(audio->analyzer, analyzer, i)
    {
        analyzer->free(analyzer);
    }
    array_free(&audio->analyzer);
    ring_buf_free(&audio->buffer);
    pthread_mutex_destroy(&audio->ctx_mutex);
}
