#ifndef __AP_AUDIO_H
#define __AP_AUDIO_H

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ap_utils.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/codec.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libavutil/mem.h"
#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "portaudio.h"

typedef struct Packet
{
    AVFrame *frame;
    enum AVSampleFormat fmt;
} Packet;

typedef struct PacketList
{
    Packet pkt;
    struct PacketList *next;
} PacketList;

typedef struct APPacketQueue
{
    PacketList *first;
    PacketList *last;
    int nb_packets;
    pthread_mutex_t mutex;
} APPacketQueue;

typedef struct AudioInternalCtx
{
    AVFormatContext *ic;
    const AVCodec *codec;
    AVCodecContext *avctx;
    int audio_stream;
} AudioInternalCtx;

typedef struct APAudioContext
{
    char *filename;
    AudioInternalCtx *it;
    int channels;
    int sample_rate;
    APPacketQueue *pkt_queue;
} APAudioContext;

APPacketQueue *ap_packet_queue_alloc();
void ap_packet_queue_free(APPacketQueue **q);
bool ap_packet_queue_empty(APPacketQueue *q);
int ap_packet_queue_enqueue(APPacketQueue *q, Packet pkt);
Packet ap_packet_queue_dequeue(APPacketQueue *q);

APAudioContext *ap_audio_alloc(const char *filename);
void ap_audio_free(APAudioContext **audioctx);
int ap_audio_init(APAudioContext *audioctx);
void ap_audio_read(APAudioContext *audioctx, APPacketQueue *pkt_queue,
                   int max_pkt_len);
void ap_audio_connect_buffer(APAudioContext *audioctx,
                             APPacketQueue *pkt_queue);

#endif /* __AP_AUDIO_H */
