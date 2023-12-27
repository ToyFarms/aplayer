#include "libdecoder.h"

/* Get codec context for the given the stream, returns NULL on fail */
const AVCodec *stream_get_codec(AVStream *stream)
{
    av_log(NULL, AV_LOG_DEBUG, "Finding codec.\n");
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not find codec.\n");
        return NULL;
    }

    return codec;
}

/* Initialize decoder for the given stream, returns -1 on fail */
int decoder_init(Decoder **dec, AVStream *stream)
{
    if (!dec)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate AVCodecContext.\n");
        return -1;
    }

    const AVCodec *codec = stream_get_codec(stream);
    if (!codec)
        return -1;

    av_log(NULL, AV_LOG_DEBUG, "Allocating AVCodecContext.\n");
    AVCodecContext *avctx = avcodec_alloc_context3(codec);
    if (!avctx)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate AVCodecContext.\n");
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Initialize AVCodecContext to use the given AVCodec.\n");
    av_log_turn_off();

    int err = avcodec_open2(avctx, codec, NULL);

    av_log_turn_on();

    if (err < 0)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not open codec. %s.\n",
               av_err2str(err));
        avcodec_free_context(&avctx);
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Allocating AVPacket.\n");
    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
    {
        av_log(NULL, AV_LOG_DEBUG, "Could not allocate AVPacket.\n");
        avcodec_free_context(&avctx);
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Allocating Decoder.\n");
    *dec = (Decoder *)malloc(sizeof(Decoder));
    if (!(*dec))
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate decoder.\n");
        avcodec_free_context(&avctx);
        av_packet_free(&pkt);
        return -1;
    }

    (*dec)->avctx = avctx;
    (*dec)->pkt = pkt;

    return 1;
}

void decoder_free(Decoder **dec)
{
    if (!dec)
        return;

    if (!(*dec))
        return;

    if ((*dec)->pkt)
    {
        av_log(NULL, AV_LOG_DEBUG, "Free decoder packet.\n");
        av_packet_free(&(*dec)->pkt);
    }

    if ((*dec)->avctx)
    {
        av_log(NULL, AV_LOG_DEBUG, "Free decoder codec context.\n");
        avcodec_free_context(&(*dec)->avctx);
    }

    free(*dec);
    *dec = NULL;
}
