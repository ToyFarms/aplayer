#include "state.h"

PlayerState *state_player_init()
{
    PlayerState *pst = (PlayerState *)malloc(sizeof(PlayerState));
    if (!pst)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate PlayerState.\n");
        return NULL;
    }

    pst->paused = false;
    pst->req_stop = false;
    pst->hide_cursor = true;

    pst->req_seek = false;
    pst->seek_incr = 0;
    pst->seek_absolute = false;

    pst->last_keypress = 0;
    pst->keypress = false;
    pst->keypress_cooldown = 0;

    pst->last_print_info = 0;
    pst->print_cooldown = ms2us(100);

    pst->timestamp = 0;

    pst->volume = 0.1f;
    pst->target_volume = 0.1f;
    pst->volume_incr = 0.01f;
    pst->volume_lerp = 0.5f;

    return pst;
}

void state_player_free(PlayerState **pst)
{
    if (!pst || !*pst)
        return;

    free(*pst);
    *pst = NULL;
}

/* Initialize StreamState, returns NULL on fail */
StreamState *state_stream_init(char *filename)
{
    StreamState *sst = (StreamState *)malloc(sizeof(StreamState));
    if (!sst)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate StreamState.\n");
        goto fail;
    }

    sst->filename = filename;

    if (stream_open(sst) < 0)
        goto fail;

    if (stream_init_stream(sst, AVMEDIA_TYPE_AUDIO) < 0)
        goto fail;

    av_log(NULL, AV_LOG_DEBUG, "Initializing SwrContext parameter.\n");
    av_channel_layout_default(&sst->audiodec->avctx->ch_layout, sst->audiodec->avctx->ch_layout.nb_channels);
    int err = swr_alloc_set_opts2(&sst->swr_ctx,
                                  &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO,
                                  AV_SAMPLE_FMT_FLT,
                                  sst->audiodec->avctx->sample_rate,
                                  &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO,
                                  sst->audiodec->avctx->sample_fmt,
                                  sst->audiodec->avctx->sample_rate,
                                  0, NULL);
    if (err < 0)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not initialize SwrContext parameter. %s.\n",
               av_err2str(err));
        goto fail;
    }

    if (!sst->swr_ctx)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Null pointer: SwrContext.\n");
        goto fail;
    }

    av_log(NULL, AV_LOG_DEBUG, "Initializing SwrContext.\n");
    if ((err = swr_init(sst->swr_ctx)) < 0)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize SwrContext. %s.\n", av_err2str(err));
        goto fail;
    }

    return sst;

fail:
    state_stream_free(&sst);

    return NULL;
}

void state_stream_free(StreamState **sst)
{
    if (!sst || !*sst)
        return;

    if ((*sst)->ic)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Close AVFormatContext.\n");
        avformat_close_input(&(*sst)->ic);
    }

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free audio decoder.\n");
    decoder_free(&(*sst)->audiodec);

    if ((*sst)->swr_ctx)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free SwrContext.\n");
        swr_free(&(*sst)->swr_ctx);
    }

    free(*sst);
    *sst = NULL;
}
