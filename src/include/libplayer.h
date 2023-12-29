#ifndef _LIBPLAYER_H
#define _LIBPLAYER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <libavutil/log.h>
#include <string.h>

#include "libhelper.h"

typedef struct PlayerState
{
    bool paused;
    bool req_exit;
    bool finished;
    bool initialized;

    bool req_seek;
    int64_t seek_incr;
    bool seek_absolute;

    int64_t timestamp;
    int64_t duration;

    bool muted;
    float volume;
    float LUFS_current_l;
    float LUFS_current_r;
    float LUFS_target;
    int LUFS_sampling_cap;
} PlayerState;

PlayerState *player_state_init();
void player_state_free(PlayerState **pst);

#endif // _LIBPLAYER_H
