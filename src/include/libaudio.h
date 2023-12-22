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

#include "libhelper.h"
#include "libstream.h"
#include "libplayer.h"

void audio_set_volume(float volume);
float audio_get_volume();
void audio_seek(float ms);
void audio_seek_to(float ms);
int64_t audio_get_timestamp();
void audio_play();
void audio_pause();
void audio_toggle_play();
void audio_exit();
void audio_wait_until_initialized();
void audio_wait_until_finished();
bool audio_is_finished();
void audio_start(char *filename, void (*finished_callback)(void));
pthread_t audio_start_async(char *filename, void (*finished_callback)(void));
void audio_free();

#endif // _LIBAUDIO_H