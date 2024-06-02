#include "ap_audio.h"

int ap_audio_init(APAudioContext *ctx, int nb_channels, int sample_rate,
                  APAudioSampleFmt sample_fmt)
{
    ctx->nb_channels = nb_channels;
    ctx->sample_rate = sample_rate;
    ctx->sample_fmt = sample_fmt;

    ctx->outctx = ap_audio_output_alloc(nb_channels, sample_rate, sample_fmt);
    if (!ctx->outctx)
        return -1;

    ctx->audio_queue = ap_queue_alloc();

    return 0;
}

void ap_audio_stop(APAudioContext *ctx)
{
    ap_audiodec_stop(ctx->decoder);
    ap_audio_output_stop(ctx->outctx);
}

void ap_audio_free(APAudioContext *ctx)
{
    ap_audio_output_free(&ctx->outctx);
    ap_audiodec_free(&ctx->decoder);
    ap_queue_free(&ctx->audio_queue);
}

bool ap_audio_register_decoder(APAudioContext *ctx, APAudioDecoder *dec)
{
    if (!ctx->decoder)
    {
        ctx->decoder = dec;
        dec->audio_queue = ctx->audio_queue;
        return true;
    }
    else
        av_log(NULL, AV_LOG_WARNING,
               "Another decoder still working on the buffer, unregister that "
               "first\n");

    return false;
}

void ap_audio_unregister_decoder(APAudioContext *ctx)
{
    ap_queue_clear(ctx->audio_queue);
    ap_audiodec_free(&ctx->decoder);
}
