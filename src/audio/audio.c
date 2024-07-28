#include "audio.h"
#include "audio_effect.h"

#include <errno.h>

int audio_common_init(audio_src *audio)
{
    audio->effects = array_create(16, sizeof(audio_effect));
    return errno;
}

void audio_common_free(audio_src *audio)
{
    array_free(&audio->effects);
}
