#include "audio_source.h"
#include "audio_effect.h"

#include <errno.h>
#include <pthread.h>

int audio_common_init(audio_source *audio)
{
    audio->pipeline = array_create(16, sizeof(audio_effect));
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
    ARR_FOREACH_BYREF(audio->pipeline, eff, i)
    {
        eff->free(eff);
    }
    array_free(&audio->pipeline);
    ring_buf_free(&audio->buffer);
    pthread_mutex_destroy(&audio->ctx_mutex);
}
