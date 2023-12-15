#include "libaudio.h"

#define _USE_SIMD

#ifdef _USE_SIMD
#include <immintrin.h>
#endif // _USE_SIMD

void audio_set_volume(AVFrame *frame, int nb_channels, float factor)
{
#ifdef _USE_SIMD
    int sample;
    __m128 scale_factor = _mm_set1_ps(factor);

    for (int ch = 0; ch < nb_channels; ch++)
    {
        for (sample = 0; sample < frame->nb_samples - 3; sample += 4)
        {
            __m128 data = _mm_load_ps(&((float *)frame->data[ch])[sample]);
            data = _mm_mul_ps(data, scale_factor);
            _mm_store_ps(&((float *)frame->data[ch])[sample], data);
        }

        for (; sample < frame->nb_samples; sample++)
        {
            ((float *)frame->data[ch])[sample] *= factor;
        }
    }
#else
    for (int ch = 0; ch < nb_channels; ch++)
    {
#pragma omp parallel for
        for (int sample = 0; sample < frame->nb_samples; sample++)
        {
            ((float *)frame->data[ch])[sample] *= factor;
        }
    }
#endif // _USE_SIMD
}
