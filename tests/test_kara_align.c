#include "base_test.h"

INCLUDE_BEGIN
#include "kara_align.h"

#define T_FRAMES 1000 /* 10 s */

/* Evidence built by hand: flat, with a clear onset peak at each of `at`
 * (seconds). `off` is a peak at each of `ends`. */
static kara_evidence *evidence(const double *at, int n, const double *ends,
                               int ne)
{
    kara_evidence *ev = calloc(1, sizeof(*ev));
    ev->T = T_FRAMES;
    ev->seconds = T_FRAMES / 100.0;
    ev->flux = calloc(T_FRAMES, sizeof(float));
    ev->peaks = calloc(T_FRAMES, sizeof(float));
    ev->off = calloc(T_FRAMES, sizeof(float));
    for (int i = 0; i < n; i++)
    {
        int f = (int)(at[i] * 100.0 + 0.5);
        ev->flux[f] = 2.5f;
        ev->peaks[f] = 2.5f;
        for (int d = 1; d <= 2; d++) /* a little skirt, as real flux has */
        {
            ev->flux[f - d] = ev->flux[f + d] = 2.5f - 0.8f * (float)d;
        }
    }
    for (int i = 0; i < ne; i++)
        ev->off[(int)(ends[i] * 100.0 + 0.5)] = 2.0f;
    return ev;
}

static void evidence_free(kara_evidence *ev)
{
    free(ev->flux);
    free(ev->peaks);
    free(ev->off);
    free(ev);
}

static int g_polls;
static bool cancel_after_3(void *ctx)
{
    (void)ctx;
    return ++g_polls > 3;
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/karaoke
 src/karaoke/kara_align.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(onsets_land_on_the_evidence)
{
    /* the line is annotated to start at 1.00 s, a little before the first
     * sung peak; four syllables, peaks at 1.10, 1.70, 2.20, 3.10 */
    const double at[] = {1.10, 1.70, 2.20, 3.10};
    const double ends[] = {3.60};
    kara_evidence *ev = evidence(at, 4, ends, 1);
    kara_params p = kara_params_default();
    float w[] = {1, 1, 1, 1};
    double on[4];
    float cf[4];
    kara_align_out o = {on, cf};

    ASSERT_TRUE(kara_align_line(ev, 1.00, 5.00, w, 4, &p, &o, NULL, NULL));
    ASSERT_FALSE(o.uniform);
    for (int i = 0; i < 4; i++)
        ASSERT_FLOAT_WITHIN((float)on[i], (float)at[i], 0.03f);
    ASSERT_TRUE(on[0] < on[1] && on[1] < on[2] && on[2] < on[3]);
    ASSERT_TRUE(o.end > on[3]);
    float with = 0;
    for (int i = 0; i < 4; i++)
    {
        ASSERT_TRUE(cf[i] > 0.0f && cf[i] <= 1.0f);
        with += cf[i] / 4.0f;
    }
    ASSERT_TRUE(cf[0] > 0.5f); /* the first is anchored to the line start too */
    evidence_free(ev);

    /* the same line over silence: the same window, nothing to be sure of */
    kara_evidence *flat = evidence(NULL, 0, NULL, 0);
    ASSERT_TRUE(kara_align_line(flat, 1.00, 5.00, w, 4, &p, &o, NULL, NULL));
    float without = 0;
    for (int i = 0; i < 4; i++)
        without += cf[i] / 4.0f;
    ASSERT_TRUE(with > without + 0.1f);
    evidence_free(flat);
}
TEST_END()

TEST_BEGIN(heavier_syllables_take_longer_when_there_is_no_evidence)
{
    /* no peaks at all: the duration prior and the line's span decide */
    kara_evidence *ev = evidence(NULL, 0, NULL, 0);
    kara_params p = kara_params_default();
    float w[] = {1, 1, 3, 1};
    double on[4];
    float cf[4];
    kara_align_out o = {on, cf};
    ASSERT_TRUE(kara_align_line(ev, 2.0, 6.0, w, 4, &p, &o, NULL, NULL));
    ASSERT_TRUE(on[0] >= 1.7 && on[0] < 2.8);
    for (int i = 1; i < 4; i++)
        ASSERT_TRUE(on[i] > on[i - 1]);
    /* the weight-3 syllable is the longest */
    ASSERT_TRUE(on[3] - on[2] > on[2] - on[1]);
    ASSERT_TRUE(on[3] < 6.0);
    evidence_free(ev);
}
TEST_END()

TEST_BEGIN(a_window_too_short_for_the_syllables_is_spread_evenly)
{
    kara_evidence *ev = evidence(NULL, 0, NULL, 0);
    kara_params p = kara_params_default();
    float w[] = {1, 1, 1, 1, 1, 1, 1, 1};
    double on[8];
    float cf[8];
    kara_align_out o = {on, cf};
    /* 8 syllables in a 0.45 s window (0.15 s and the 0.30 s pre-roll) cannot
     * each last the 60 ms minimum */
    ASSERT_TRUE(kara_align_line(ev, 1.0, 1.15, w, 8, &p, &o, NULL, NULL));
    ASSERT_TRUE(o.uniform);
    for (int i = 1; i < 8; i++)
        ASSERT_TRUE(on[i] > on[i - 1]);
    ASSERT_TRUE(on[0] == 1.0);
    ASSERT_FLOAT_EQ(cf[0], 0.0f);
    evidence_free(ev);
}
TEST_END()

TEST_BEGIN(uniform_spread_follows_the_weights)
{
    float w[] = {1, 3};
    double on[2];
    float cf[2];
    kara_align_out o = {on, cf};
    kara_align_uniform(0.0, 4.3, w, 2, &o);
    /* 4.0 s to share, 1:3 */
    ASSERT_FLOAT_WITHIN((float)on[1], 1.0f, 0.001f);
    ASSERT_FLOAT_WITHIN((float)o.end, 4.0f, 0.001f);
}
TEST_END()

TEST_BEGIN(a_cancelled_alignment_returns_false)
{
    const double at[] = {1.1, 1.7, 2.2, 3.1};
    kara_evidence *ev = evidence(at, 4, NULL, 0);
    kara_params p = kara_params_default();
    float w[] = {1, 1, 1, 1};
    double on[4];
    float cf[4];
    kara_align_out o = {on, cf};
    g_polls = 0;
    ASSERT_FALSE(kara_align_line(ev, 1.0, 5.0, w, 4, &p, &o, cancel_after_3, NULL));
    evidence_free(ev);
}
TEST_END()

TEST_BEGIN(a_long_line_is_not_a_problem)
{
    /* 60 syllables in 20 s: the lattice is big, the result still ordered */
    kara_evidence *ev = calloc(1, sizeof(*ev));
    ev->T = T_FRAMES;
    ev->seconds = 10;
    ev->flux = calloc(T_FRAMES, sizeof(float));
    ev->peaks = calloc(T_FRAMES, sizeof(float));
    ev->off = calloc(T_FRAMES, sizeof(float));
    for (int f = 40; f < 900; f += 15)
        ev->flux[f] = ev->peaks[f] = 2.0f;
    kara_params p = kara_params_default();
    float w[60];
    double on[60];
    float cf[60];
    for (int i = 0; i < 60; i++)
        w[i] = 1.0f;
    kara_align_out o = {on, cf};
    ASSERT_TRUE(kara_align_line(ev, 0.5, 9.5, w, 60, &p, &o, NULL, NULL));
    for (int i = 1; i < 60; i++)
        ASSERT_TRUE(on[i] > on[i - 1]);
    ASSERT_TRUE(on[59] < 9.5);
    evidence_free(ev);
}
TEST_END()
