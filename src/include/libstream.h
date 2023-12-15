#ifndef _LIBSTREAM_H
#define _LIBSTREAM_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "struct.h"
#include "libdecoder.h"

int stream_open(StreamState *sst);
int stream_open_input(StreamState *sst);
int stream_alloc_context(StreamState *sst);
int stream_get_info(StreamState *sst);
int stream_init_stream(StreamState *sst, enum AVMediaType media_type);
int stream_get_input_format(StreamState *sst, char *filename);

#endif // _LIBSTREAM_H