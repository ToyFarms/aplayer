#ifndef _LIBSTREAM_H
#define _LIBSTREAM_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

#include "libdecoder.h"
#include "libhelper.h"

typedef struct StreamState
{
    char *filename;

    AVFormatContext *ic;
    const AVInputFormat *iformat;
    SwrContext *swr_ctx;
    AVFrame *frame;
    AVFrame *swr_frame;

    AVStream *audio_stream;
    Decoder *audiodec;
    int audio_stream_index;
} StreamState;

StreamState *stream_state_init(char *filename);
void stream_state_free(StreamState **sst);

int stream_open(StreamState *sst);
int stream_open_input(StreamState *sst);
int stream_alloc_context(StreamState *sst);
int stream_get_info(StreamState *sst);
int stream_init_stream(StreamState *sst, enum AVMediaType media_type);
int stream_get_input_format(StreamState *sst, char *filename);
int stream_init_swr(StreamState *sst);

#endif // _LIBSTREAM_H