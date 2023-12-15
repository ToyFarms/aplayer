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

/* Initialize decoder for the given stream, returns NULL on fail */
int decoder_init(Decoder **dec, AVStream *stream)
{
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
    int err = avcodec_open2(avctx, codec, NULL);
    if (err < 0)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not open codec. %s.\n",
               av_err2str(err));
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Allocating AVPacket.\n");
    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
    {
        av_log(NULL, AV_LOG_DEBUG, "Could not allocate AVPacket.\n");
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Initializing Decoder.\n");
    *dec = (Decoder *)malloc(sizeof(Decoder));
    if (!dec)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate decoder.\n");
        return -1;
    }

    (*dec)->avctx = avctx;
    (*dec)->pkt = pkt;

    return 1;
}

void decoder_free(Decoder *dec)
{
    if (!dec)
        return;
    av_log(NULL, AV_LOG_DEBUG, "Free decoder packet.\n");
    av_packet_free(&dec->pkt);
    av_log(NULL, AV_LOG_DEBUG, "Free decoder codec context.\n");
    avcodec_free_context(&dec->avctx);

    free(dec);
    dec = NULL;
}
