#ifndef __AP_AUDIO_H
#define __AP_AUDIO_H

#include "ap_audio_dec.h"
#include "ap_audio_output.h"
#include "ap_queue.h"

typedef struct APAudioContext
{
    APQueue *audio_queue;
    APAudioDecoder *decoder;
    APAudioOutputContext *outctx;
    int nb_channels;
    int sample_rate;
    APAudioSampleFmt sample_fmt;
    int stop_signal;
} APAudioContext;

int ap_audio_init(APAudioContext *ctx, int nb_channels, int sample_rate,
                  APAudioSampleFmt sample_fmt);
void ap_audio_stop(APAudioContext *ctx);
void ap_audio_free(APAudioContext *ctx);
bool ap_audio_register_decoder(APAudioContext *ctx, APAudioDecoder *dec);
void ap_audio_unregister_decoder(APAudioContext *ctx);

#endif /* __AP_AUDIO_H */
