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

    memset(pst, 0, sizeof(pst));

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
    pst->duration = 0;

    pst->muted = false;
    pst->volume = 1.0f;
    pst->LUFS_target = -14.0f;
    pst->LUFS_sampling_cap = 50000;

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