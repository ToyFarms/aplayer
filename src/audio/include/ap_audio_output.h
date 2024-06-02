#ifndef __AP_AUDIO_OUTPUT_H
#define __AP_AUDIO_OUTPUT_H

#include "ap_audio_processing.h"
#include "ap_queue.h"
#include "ap_utils.h"
#include "libavutil/log.h"
#include "libavutil/samplefmt.h"
#include "portaudio.h"
#include "ap_audio_sample.h"

#include <stdlib.h>

typedef struct APAudioOutputContext
{
    PaStream *stream;
    int channels;
    int sample_rate;
    APAudioSampleFmt ap_sample_fmt;
    PaSampleFormat pa_sample_fmt;
    enum AVSampleFormat av_sample_fmt;
    bool stop_signal;
} APAudioOutputContext;

int ap_audio_output_init(APAudioOutputContext *ctx, int nb_channels,
                         int sample_rate, APAudioSampleFmt sample_fmt);
APAudioOutputContext *ap_audio_output_alloc(int nb_channels, int sample_rate,
                                            APAudioSampleFmt sample_fmt);
void ap_audio_output_free(APAudioOutputContext **audio_outctx);
void ap_audio_output_start(APAudioOutputContext *audio_outctx,
                           APQueue *audio_queue);
void ap_audio_output_stop(APAudioOutputContext *audio_outctx);
pthread_t ap_audio_output_start_async(APAudioOutputContext *audio_outctx,
                                      APQueue *audio_queue);

#endif /* __AP_AUDIO_OUTPUT_H */
