#include "audio_spectrum.h"

#include <math.h>
#include <stddef.h>

#define MAG_FLOOR 1e-9f /* log10 of nothing */

audio_spectrum_params audio_spectrum_defaults(void)
{
    return (audio_spectrum_params){
        /* a little wider than the audible range, so the ends of the picture
         * fall away to the floor instead of stopping at a hard edge (f_hi is
         * still cut at 98% of Nyquist; the tilt pulls the ends down) */
        .f_lo = 10.0f,
        .f_hi = 24000.0f,
        .db_floor = -90.0f,
        .db_ceil = -9.0f,
        .tilt_db_oct = 4.5f,
    };
}

/* top of the axis: the requested one, but never beyond what the signal has */
static float f_top(const audio_spectrum_params *p, int sample_rate)
{
    float nyquist = (float)sample_rate * 0.5f;
    return fminf(p->f_hi, nyquist * 0.98f);
}

float audio_spectrum_band_hz(const audio_spectrum_params *p, int sample_rate,
                             int i, int nbands)
{
    float hi = f_top(p, sample_rate);
    if (nbands < 2 || hi <= p->f_lo)
        return p->f_lo;
    float t = (float)i / (float)(nbands - 1);
    return p->f_lo * powf(hi / p->f_lo, t);
}

/* amplitude at a fractional bin position */
static float mag_at(const float *mag, int nbins, float pos)
{
    if (pos <= 0.0f)
        return mag[0];
    if (pos >= (float)(nbins - 1))
        return mag[nbins - 1];
    int i = (int)pos;
    float t = pos - (float)i;
    return mag[i] + (mag[i + 1] - mag[i]) * t;
}

void audio_spectrum_bands(const float *mag, int nbins, int sample_rate,
                          const audio_spectrum_params *p, float *out,
                          int nbands)
{
    if (out == NULL || nbands <= 0)
        return;
    if (mag == NULL || nbins < 2 || sample_rate <= 0 || p->f_lo <= 0.0f ||
        f_top(p, sample_rate) <= p->f_lo || p->db_ceil <= p->db_floor)
    {
        for (int i = 0; i < nbands; i++)
            out[i] = 0.0f;
        return;
    }

    float df = (float)sample_rate / (2.0f * (float)(nbins - 1)); /* Hz / bin */
    float hi = f_top(p, sample_rate);
    /* half a band, as a frequency ratio */
    float half = nbands > 1 ? powf(hi / p->f_lo, 0.5f / (float)(nbands - 1))
                            : 1.0f;

    for (int i = 0; i < nbands; i++)
    {
        float fc = audio_spectrum_band_hz(p, sample_rate, i, nbands);
        float b_lo = fc / half / df, b_hi = fc * half / df;

        float amp;
        if (b_hi - b_lo < 1.0f)
            amp = mag_at(mag, nbins, fc / df);
        else
        {
            int k0 = (int)floorf(b_lo + 0.5f), k1 = (int)floorf(b_hi + 0.5f);
            if (k0 < 0)
                k0 = 0;
            if (k1 > nbins - 1)
                k1 = nbins - 1;
            amp = 0.0f;
            for (int k = k0; k <= k1; k++)
                if (mag[k] > amp)
                    amp = mag[k];
        }

        float db = 20.0f * log10f(fmaxf(amp, MAG_FLOOR)) +
                   p->tilt_db_oct * log2f(fc / 1000.0f);
        float v = (db - p->db_floor) / (p->db_ceil - p->db_floor);
        out[i] = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    }
}
