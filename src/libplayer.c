#include "libplayer.h"

PlayerState *player_state_init()
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing PlayerState.\n");

    PlayerState *pst = (PlayerState *)malloc(sizeof(PlayerState));
    if (!pst)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate PlayerState.\n");
        return NULL;
    }

    memset(pst, 0, sizeof(PlayerState));

    pst->paused = false;
    pst->req_exit = false;
    pst->finished = false;
    pst->initialized = false;

    pst->req_seek = false;
    pst->seek_incr = 0;
    pst->seek_absolute = false;

    pst->last_print_info = 0;
    pst->print_cooldown = ms2us(100);

    pst->timestamp = 0;

    pst->volume = 0.1f;
    pst->volume_target = 0.1f;
    pst->volume_incr = 0.05f;
    pst->volume_lerp = 0.8f;
    pst->volume_max = 2.0f;

    return pst;
}

void player_state_free(PlayerState **pst)
{
    av_log(NULL, AV_LOG_DEBUG, "Free PlayerState.\n");

    if (!pst)
        return;

    if (!(*pst))
        return;

    free(*pst);
    *pst = NULL;
}