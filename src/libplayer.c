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

    memset(pst, 0, sizeof(*pst));

    pst->paused = false;
    pst->req_exit = false;
    pst->finished = false;
    pst->initialized = false;

    pst->req_seek = false;
    pst->seek_incr = 0;
    pst->seek_absolute = false;

    pst->timestamp = 0;
    pst->duration = 0;

    pst->muted = false;
    pst->volume = 1.0f;
    // TODO: Use config file for settings like these
    pst->LUFS_target = -20.0f;
    pst->LUFS_avg = pst->LUFS_target;
    pst->LUFS_current_l = pst->LUFS_target;
    pst->LUFS_current_r = pst->LUFS_target;
    pst->LUFS_calculation_progress = 0;
    pst->LUFS_sampling_cap = MSTOUS(60000 * 20);
    pst->frame = NULL;

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