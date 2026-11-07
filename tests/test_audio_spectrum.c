#include "base_test.h"

INCLUDE_BEGIN
#include "audio_spectrum.h"
#include <math.h>

#define NB    256
#define FFT_N 8192
#define SR    48000
#define BINS  (FFT_N / 2 + 1)

/* A spectrum with one tone of amplitude `amp` (1.0 = full scale) at `hz`,
 * on the bin it is closest to. */
static void tone(float *mag, float hz, float amp)
{
    for (int k = 0; k < BINS; k++)
        mag[k] = 0.0f;
    int k = (int)lroundf(hz / ((float)SR / (float)FFT_N));
    mag[k] = amp;
}

static int loudest(const float *band, int n)
{
    int best = 0;
    for (int i = 1; i < n; i++)
        if (band[i] > band[best])
            best = i;
    return best;
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 src/audio/analyzer/audio_spectrum.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(a_tone_lands_on_its_frequency)
{
    static float mag[BINS];
    float band[NB];
    audio_spectrum_params p = audio_spectrum_defaults();
    p.tilt_db_oct = 0.0f;

    const float hz[] = {60.0f, 440.0f, 1000.0f, 5000.0f, 14000.0f};
    for (size_t t = 0; t < sizeof(hz) / sizeof(hz[0]); t++)
    {
        tone(mag, hz[t], 0.1f);
        audio_spectrum_bands(mag, BINS, SR, &p, band, NB);
        float at = audio_spectrum_band_hz(&p, SR, loudest(band, NB), NB);
        /* within a bin of where the tone really is, which is all an FFT
         * can say at the bass */
        ASSERT_TRUE(fabsf(at - hz[t]) < hz[t] * 0.08f + 6.0f);
    }
}
TEST_END()

TEST_BEGIN(levels_follow_decibels_between_floor_and_ceiling)
{
    static float mag[BINS];
    float band[NB];
    audio_spectrum_params p = audio_spectrum_defaults();
    p.tilt_db_oct = 0.0f;
    p.db_floor = -80.0f;
    p.db_ceil = 0.0f;

    tone(mag, 1000.0f, powf(10.0f, -40.0f / 20.0f)); /* -40 dB */
    audio_spectrum_bands(mag, BINS, SR, &p, band, NB);
    ASSERT_FLOAT_WITHIN(band[loudest(band, NB)], 0.5f, 0.02f);

    tone(mag, 1000.0f, 1.0f); /* full scale */
    audio_spectrum_bands(mag, BINS, SR, &p, band, NB);
    ASSERT_FLOAT_WITHIN(band[loudest(band, NB)], 1.0f, 0.001f);

    tone(mag, 1000.0f, powf(10.0f, -100.0f / 20.0f)); /* under the floor */
    audio_spectrum_bands(mag, BINS, SR, &p, band, NB);
    ASSERT_FLOAT_WITHIN(band[loudest(band, NB)], 0.0f, 0.001f);
}
TEST_END()

TEST_BEGIN(a_tone_is_as_tall_in_the_bass_as_in_the_treble)
{
    static float mag[BINS];
    float lo[NB], hi[NB];
    audio_spectrum_params p = audio_spectrum_defaults();
    p.tilt_db_oct = 0.0f;

    tone(mag, 110.0f, 0.05f);
    audio_spectrum_bands(mag, BINS, SR, &p, lo, NB);
    tone(mag, 10000.0f, 0.05f);
    audio_spectrum_bands(mag, BINS, SR, &p, hi, NB);

    /* in the bass the bands are finer than the bins, so the nearest band
     * may sit a little beside the bin: allow a couple of dB */
    float span = p.db_ceil - p.db_floor;
    ASSERT_FLOAT_WITHIN(lo[loudest(lo, NB)], hi[loudest(hi, NB)], 3.0f / span);
}
TEST_END()

TEST_BEGIN(the_tilt_lifts_the_treble_and_leaves_one_khz_alone)
{
    static float mag[BINS];
    float a[NB], b[NB], c[NB];
    audio_spectrum_params flat = audio_spectrum_defaults();
    flat.tilt_db_oct = 0.0f;
    audio_spectrum_params tilt = audio_spectrum_defaults();
    float span = tilt.db_ceil - tilt.db_floor;

    tone(mag, 1000.0f, 0.01f);
    audio_spectrum_bands(mag, BINS, SR, &flat, a, NB);
    audio_spectrum_bands(mag, BINS, SR, &tilt, b, NB);
    ASSERT_FLOAT_WITHIN(b[loudest(b, NB)], a[loudest(a, NB)], 0.5f / span);

    tone(mag, 8000.0f, 0.01f); /* three octaves up */
    audio_spectrum_bands(mag, BINS, SR, &tilt, c, NB);
    ASSERT_FLOAT_WITHIN(c[loudest(c, NB)] - b[loudest(b, NB)],
                        3.0f * tilt.tilt_db_oct / span, 1.0f / span);
}
TEST_END()

TEST_BEGIN(silence_and_bad_input_give_a_flat_floor)
{
    static float mag[BINS];
    float band[NB];
    audio_spectrum_params p = audio_spectrum_defaults();

    for (int i = 0; i < NB; i++)
        band[i] = 0.7f;
    audio_spectrum_bands(mag, BINS, SR, &p, band, NB); /* all zero */
    for (int i = 0; i < NB; i++)
        ASSERT_FLOAT_WITHIN(band[i], 0.0f, 0.0001f);

    for (int i = 0; i < NB; i++)
        band[i] = 0.7f;
    audio_spectrum_bands(NULL, BINS, SR, &p, band, NB);
    audio_spectrum_bands(mag, 1, SR, &p, band, NB);
    audio_spectrum_bands(mag, BINS, 0, &p, band, NB);
    for (int i = 0; i < NB; i++)
        ASSERT_FLOAT_WITHIN(band[i], 0.0f, 0.0001f);
    audio_spectrum_bands(mag, BINS, SR, &p, NULL, NB); /* must not crash */
}
TEST_END()

TEST_BEGIN(the_axis_is_logarithmic_and_stops_at_nyquist)
{
    audio_spectrum_params p = audio_spectrum_defaults();

    /* 48 kHz: the top is 98% of Nyquist, which is below f_hi */
    float top = 0.98f * 24000.0f;
    ASSERT_FLOAT_WITHIN(audio_spectrum_band_hz(&p, SR, 0, NB), 10.0f, 0.01f);
    ASSERT_FLOAT_WITHIN(audio_spectrum_band_hz(&p, SR, NB - 1, NB), top, 0.5f);
    /* the middle band is the geometric mean: an octave is as wide on screen
     * at 100 Hz as at 10 kHz */
    float mid = audio_spectrum_band_hz(&p, SR, (NB - 1) / 2, NB);
    ASSERT_FLOAT_WITHIN(mid, sqrtf(10.0f * top), 40.0f);

    /* a 22.05 kHz file has nothing above 11 kHz to show */
    float top22 = audio_spectrum_band_hz(&p, 22050, NB - 1, NB);
    ASSERT_TRUE(top22 < 11025.0f && top22 > 10000.0f);
}
TEST_END()
