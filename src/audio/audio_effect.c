#include "audio_effect.h"

#include <stdlib.h>

void _audio_eff_free_default(audio_effect *eff)
{
    if (!eff)
        return;

    if (eff->ctx != NULL)
        free(eff->ctx);
    eff->ctx = NULL;
}
