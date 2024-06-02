#ifndef __AP_AUDIO_DEC_H
#define __AP_AUDIO_DEC_H

#include "ap_audio_sample.h"
#include "ap_queue.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/codec.h"
#include "libavformat/avformat.h"
#include "portaudio.h"

#include <pthread.h>
#include <stdbool.h>

typedef struct AudioInternalCtx
{
    AVFormatContext *ic;
    const AVCodec *codec;
    AVCodecContext *avctx;
    int audio_stream;
} AudioInternalCtx;

typedef struct APAudioDecoder
{
    char *filename;
    AudioInternalCtx *it;
    int channels;
    int sample_rate;
    int wanted_nb_channels;
    int wanted_sample_rate;
    APAudioSampleFmt wanted_ap_sample_fmt;
    PaSampleFormat wanted_pa_sample_fmt;
    enum AVSampleFormat wanted_av_sample_fmt;
    APQueue *audio_queue;
    bool stop_signal;
} APAudioDecoder;

APAudioDecoder *ap_audiodec_alloc(const char *filename, int wanted_nb_channels,
                                  int wanted_sample_rate,
                                  APAudioSampleFmt wanted_sample_fmt);
void ap_audiodec_free(APAudioDecoder **audioctx);
int ap_audiodec_init(APAudioDecoder *audioctx);
void ap_audiodec_decode(APAudioDecoder *audioctx, int max_pkt_len);
void ap_audiodec_stop(APAudioDecoder *audioctx);
pthread_t ap_audiodec_decode_async(APAudioDecoder *audioctx, int max_pkt_len);

#endif /* __AP_AUDIO_DEC_H */
