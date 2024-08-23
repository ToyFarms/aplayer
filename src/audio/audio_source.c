#include "audio_source.h"
#include "audio_effect.h"

#include <errno.h>

int audio_common_init(audio_source *audio)
{
    audio->effects = array_create(16, sizeof(audio_effect));
    audio->buffer = ring_buf_create(audio->target_sample_rate * 10, sizeof(float));
    return errno;
}

void audio_common_free(audio_source *audio)
{
    array_free(&audio->effects);
    ring_buf_free(&audio->buffer);
}
