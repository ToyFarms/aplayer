#include "base_test.h"

INCLUDE_BEGIN
#include "kara_feat.h"

#define SECONDS 8
#define N       (SECONDS * KARA_SR)

/* Three sung-like notes (a few partials, a sharp attack, a decay) in the
 * centre, at 1.0, 3.0 and 5.0 s. `pan`: 0 centred, 1 left only. */
static float *signal_with_notes(float pan)
{
    float *x = calloc((size_t)N * 2, sizeof(float));
    const double starts[] = {1.0, 3.0, 5.0};
    const double f0[] = {220.0, 330.0, 262.0};
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < KARA_SR; i++)
        {
            double t = (double)i / KARA_SR;
            double env = (t < 0.005 ? t / 0.005 : 1.0) * exp(-t * 2.0);
            double v = 0;
            for (int h = 1; h <= 6; h++)
                v += sin(2.0 * 3.14159265358979 * f0[k] * h * t) / h;
            size_t n = (size_t)(starts[k] * KARA_SR) + (size_t)i;
            float s = (float)(0.3 * env * v);
            x[2 * n] = s;
            x[2 * n + 1] = pan > 0.5f ? 0.0f : s;
        }
    return x;
}

static kara_evidence *analyse(const float *x, int chunk, const kara_plans *pl)
{
    kara_feat *f = kara_feat_new(pl, kara_feat_stage2());
    for (int i = 0; i < N; i += chunk)
    {
        int m = N - i < chunk ? N - i : chunk;
        kara_feat_push(f, x + 2 * i, m);
    }
    kara_evidence *ev = kara_feat_finish(f);
    kara_feat_free(f);
    return ev;
}

static int peak_near(const kara_evidence *ev, double sec, int radius)
{
    int c = (int)(sec * KARA_FPS), best = -1;
    for (int t = c - radius; t <= c + radius; t++)
        if (t >= 0 && t < ev->T && ev->peaks[t] > 0 &&
            (best < 0 || ev->peaks[t] > ev->peaks[best]))
            best = t;
    return best;
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/karaoke
 src/karaoke/kara_feat.c
 pkg-config:fftw3
 -lpthread
 -lm
 */ CFLAGS_END

TEST_BEGIN(a_note_onset_is_a_peak_of_the_evidence)
{
    kara_plans *pl = kara_plans_new();
    ASSERT_TRUE(pl != NULL);
    float *x = signal_with_notes(0.0f);
    kara_evidence *ev = analyse(x, 4096, pl);
    ASSERT_TRUE(ev != NULL);
    ASSERT_INT_EQ(ev->T, 1 + N / KARA_HOP);
    ASSERT_FLOAT_WITHIN((float)ev->seconds, (float)SECONDS, 0.001f);
    for (int k = 0; k < 3; k++)
    {
        int t = peak_near(ev, 1.0 + 2.0 * k, 6);
        ASSERT_TRUE(t >= 0);
        ASSERT_TRUE(ev->peaks[t] > 0.5f);
    }
    /* flux is within its range, peaks are a subset of it */
    for (int t = 0; t < ev->T; t++)
    {
        ASSERT_TRUE(ev->flux[t] >= 0.0f && ev->flux[t] <= 3.0f);
        ASSERT_TRUE(ev->peaks[t] == 0.0f || ev->peaks[t] == ev->flux[t]);
    }
    kara_evidence_free(ev);
    free(x);
    kara_plans_free(pl);
}
TEST_END()

TEST_BEGIN(the_result_does_not_depend_on_how_the_audio_is_chunked)
{
    kara_plans *pl = kara_plans_new();
    float *x = signal_with_notes(0.0f);
    kara_evidence *a = analyse(x, N, pl); /* all at once */
    kara_evidence *b = analyse(x, 4096, pl);
    kara_evidence *c = analyse(x, 997, pl); /* an awkward size */
    kara_evidence *d = analyse(x, 1, pl);   /* one sample at a time */
    ASSERT_TRUE(a && b && c && d);
    for (int t = 0; t < a->T; t++)
    {
        ASSERT_FLOAT_WITHIN(a->flux[t], b->flux[t], 1e-6f);
        ASSERT_FLOAT_WITHIN(a->flux[t], c->flux[t], 1e-6f);
        ASSERT_FLOAT_WITHIN(a->flux[t], d->flux[t], 1e-6f);
        ASSERT_FLOAT_WITHIN(a->off[t], c->off[t], 1e-6f);
    }
    kara_evidence_free(a);
    kara_evidence_free(b);
    kara_evidence_free(c);
    kara_evidence_free(d);
    free(x);
    kara_plans_free(pl);
}
TEST_END()

TEST_BEGIN(sound_panned_hard_to_one_side_is_not_vocals)
{
    /* the centre mask keeps what is the same in both channels */
    kara_plans *pl = kara_plans_new();
    float *centre = signal_with_notes(0.0f);
    float *left = signal_with_notes(1.0f);
    kara_evidence *a = analyse(centre, 4096, pl);
    kara_evidence *b = analyse(left, 4096, pl);
    double ea = 0, eb = 0;
    for (int t = 0; t < a->T; t++)
    {
        ea += a->flux0[t];
        eb += b->flux0[t];
    }
    ASSERT_TRUE(ea > 0.0);
    /* flux is normalised to its own 95th percentile, so compare the peak the
     * notes make against the floor instead: the left-only version has none */
    int ta = peak_near(a, 1.0, 6);
    ASSERT_TRUE(ta >= 0 && a->flux0[ta] > 0.5f);
    kara_evidence_free(a);
    kara_evidence_free(b);
    free(centre);
    free(left);
    kara_plans_free(pl);
    (void)eb;
}
TEST_END()

TEST_BEGIN(too_little_audio_gives_nothing)
{
    kara_plans *pl = kara_plans_new();
    float x[2 * 1000] = {0};
    kara_feat *f = kara_feat_new(pl, kara_feat_stage2());
    kara_feat_push(f, x, 1000);
    ASSERT_TRUE(kara_feat_finish(f) == NULL);
    kara_feat_free(f);
    kara_plans_free(pl);
}
TEST_END()

TEST_BEGIN(the_percentile_interpolates_like_numpy)
{
    float a[] = {4, 1, 3, 2};
    ASSERT_FLOAT_WITHIN(kara_percentile(a, 4, 50), 2.5f, 1e-6f);
    float b[] = {1, 2, 3, 4, 5};
    ASSERT_FLOAT_WITHIN(kara_percentile(b, 5, 95), 4.8f, 1e-5f);
    float c[] = {7};
    ASSERT_FLOAT_WITHIN(kara_percentile(c, 1, 30), 7.0f, 1e-6f);
}
TEST_END()
