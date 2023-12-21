#ifndef _LIBPLAYER_H
#define _LIBPLAYER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <libavutil/log.h>

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

    int64_t last_print_info;
    int64_t print_cooldown;

    int64_t timestamp;

    float volume;
    float volume_incr;
    float volume_lerp;
    float volume_max;
    float lufs_target;
    int lufs_sample_hard_cap;
} PlayerState;

PlayerState *player_state_init();
void player_state_free(PlayerState **pst);

#endif // _LIBPLAYER_H
