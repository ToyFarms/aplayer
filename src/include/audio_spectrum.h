#ifndef __AUDIO_SPECTRUM_H
#define __AUDIO_SPECTRUM_H

/*
 * From an FFT magnitude spectrum to what a spectrum analyzer draws: a fixed
 * number of bands spaced evenly on a log-frequency axis (every octave gets the
 * same width, as in a DAW), each one a level on a dB scale in 0..1.
 *
 * Pure arithmetic, no audio thread or FFT library involved.
 */

typedef struct audio_spectrum_params
{
    float f_lo, f_hi;  /* Hz covered by the bands (f_hi is cut at Nyquist) */
    float db_floor;    /* level that maps to 0 */
    float db_ceil;     /* level that maps to 1 */
    float tilt_db_oct; /* dB added per octave above 1 kHz (taken away below).
                          Music falls off towards the treble; without a tilt
                          the right half of the picture would sit on the
                          floor. */
} audio_spectrum_params;

audio_spectrum_params audio_spectrum_defaults(void);

/*
 * `mag` holds `nbins` amplitudes (nbins = fft_size / 2 + 1, bin k is
 * k * sample_rate / fft_size Hz) where 1.0 is a full-scale sine. Writes
 * `nbands` values in 0..1 to `out`, lowest frequency first.
 *
 * A band that spans several bins shows the loudest of them, so a tone looks
 * the same height anywhere on the axis; below that (the bass, where bins are
 * wider than bands) the bins are interpolated.
 */
void audio_spectrum_bands(const float *mag, int nbins, int sample_rate,
                          const audio_spectrum_params *p, float *out,
                          int nbands);

/* Centre frequency of band `i` of `nbands`, for labels and tests. */
float audio_spectrum_band_hz(const audio_spectrum_params *p, int sample_rate,
                             int i, int nbands);

#endif /* __AUDIO_SPECTRUM_H */
