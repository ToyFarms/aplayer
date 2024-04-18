#ifndef __AP_AUDIO_OUTPUT_H
#define __AP_AUDIO_OUTPUT_H

#include "ap_audio.h"
#include "ap_audio_processing.h"
#include "ap_utils.h"
#include "libavutil/log.h"
#include "portaudio.h"

#include <stdlib.h>

typedef struct APAudioOutputContext
{
    PaStream *stream;
    int channels;
    int sample_rate;
    PaSampleFormat sample_fmt;
    enum AVSampleFormat av_sample_fmt;
} APAudioOutputContext;

APAudioOutputContext *ap_audio_output_alloc(int nb_channels, int sample_rate,
                                            PaSampleFormat sample_fmt);
void ap_audio_output_free(APAudioOutputContext **audio_outctx);
void ap_audio_output_close();
void ap_audio_output_start_stream(APAudioOutputContext *audio_outctx,
                                  APPacketQueue *pkt_queue);

#endif /* __AP_AUDIO_OUTPUT_H */
