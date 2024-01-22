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
#include <stdbool.h>
#include <pthread.h>
#include <immintrin.h>

#include "libhelper.h"
#include "libstream.h"
#include "libplayer.h"

void audio_set_volume(float volume);
float audio_get_volume();
void audio_mute();
void audio_unmute();
bool audio_is_muted();
void audio_seek(int64_t us);
void audio_seek_to(int64_t us);
int64_t audio_get_timestamp();
int64_t audio_get_duration();
void audio_play();
void audio_pause();
void audio_toggle_play();
void audio_exit();
void audio_wait_until_initialized();
void audio_wait_until_finished();
bool audio_is_finished();
bool audio_is_initialized();
bool audio_is_paused();
void audio_start(char *filename, void (*finished_callback)(void));
pthread_t audio_start_async(char *filename, void (*finished_callback)(void));
PlayerState *audio_init();
void audio_free();
StreamState *_audio_get_stream();

#endif // _LIBAUDIO_H