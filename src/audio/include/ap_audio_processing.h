#ifndef __AP_AUDIO_PROCESSING_H
#define __AP_AUDIO_PROCESSING_H

void ap_audio_proc_gain(float *signal, int len, float factor);
double ap_audio_proc_rms(float *signal, int len);

#endif /* __AP_AUDIO_PROCESSING_H */
