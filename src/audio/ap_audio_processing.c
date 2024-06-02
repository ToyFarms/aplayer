#include "ap_audio_processing.h"

#include <math.h>

#define DBTOFACTOR(x) (powf(10.0f, (x) / 20.0f))
#define FACTORTODB(x) (20 * log10f((x)))

void ap_audio_proc_gain(float *signal, int len, float factor)
{
    for (int i = 0; i < len; i++)
        signal[i] *= factor;
}

double ap_audio_proc_rms(float *signal, int len)
{
    double sum = 0.0;

    for (int i = 0; i < len; i++)
        sum += signal[i] * signal[i];

    return len != 0 ? sqrt(sum / len) : 0.0;
}
