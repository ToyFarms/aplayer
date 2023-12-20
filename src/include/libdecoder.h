#ifndef _LIBDECODER_H
#define _LIBDECODER_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "libhelper.h"

typedef struct Decoder
{
    AVCodecContext *avctx;
    AVPacket *pkt;
} Decoder;

int decoder_init(Decoder **dec, AVStream *stream);
void decoder_free(Decoder **dec);

#endif // _LIBDECODER_H