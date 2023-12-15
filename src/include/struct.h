#ifndef _STRUCT_H
#define _STRUCT_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <stdbool.h>

typedef struct Decoder
{
    AVCodecContext *avctx;
    AVPacket *pkt;
} Decoder;

typedef struct StreamState
{
    char *filename;

    AVFormatContext *ic;
    const AVInputFormat *iformat;
    SwrContext *swr_ctx;

    AVStream *audio_stream;
    Decoder *audiodec;
    int audio_stream_index;
} StreamState;

typedef struct PlayerState
{
    bool paused;

    bool req_seek;
    int64_t seek_incr;

    int64_t last_keypress;
    bool keypress;
    int64_t keypress_cooldown;

    int64_t timestamp;

    float volume;
    float target_volume;
    float volume_increment;
    float volume_lerp;

    bool hide_cursor;
} PlayerState;

#endif // _STRUCT_H