#ifndef _LIBAUDIO_H
#define _LIBAUDIO_H

#include <portaudio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <math.h>
#include <pthread.h>

#include "libaudio.h"
#include "libhelper.h"
#include "state.h"

#define _USE_SIMD

void audio_set_volume(float volume);
void audio_seek_to(float ms);
void audio_seek(float ms);
PlayerState *audio_get_state();
void audio_play();
void audio_pause();
void audio_toggle_play();
void audio_stop();
void audio_start(char *filename);
pthread_t audio_start_async(char *filename);
void audio_wait_until_initialized();
void audio_wait_until_finished();
bool audio_is_finished();

#endif // _LIBAUDIO_H