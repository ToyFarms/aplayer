#ifndef _LIBAUDIO_H
#define _LIBAUDIO_H

#include <libavformat/avformat.h>
#include <math.h>

void audio_set_volume(AVFrame *frame, int nb_channels, float factor);

#endif // _LIBAUDIO_H