#include "base_test.h"

INCLUDE_BEGIN
#include "audio_scope.h"
#include <math.h>

static audio_scope g_scope; /* 128 KiB: not for the stack */

/* `n` mono frames of a ramp that starts at `first` and rises by 1 each */
static void push_ramp(audio_scope *s, int first, int n)
{
    static float tmp[3 * AUDIO_SCOPE_SIZE];
    for (int i = 0; i < n; i++)
        tmp[i] = (float)(first + i);
    audio_scope_push(s, tmp, n, 1, 48000);
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 src/audio/analyzer/audio_scope.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(a_pushed_block_reads_back)
{
    audio_scope_reset(&g_scope);
    push_ramp(&g_scope, 0, 100);
    ASSERT_INT_EQ((int)g_scope.head, 100);
    ASSERT_INT_EQ(g_scope.sample_rate, 48000);

    float out[10];
    audio_scope_read(&g_scope, 90, out, 10);
    for (int i = 0; i < 10; i++)
        ASSERT_FLOAT_EQ(out[i], (float)(90 + i));
}
TEST_END()

TEST_BEGIN(blocks_join_up_across_the_wrap)
{
    audio_scope_reset(&g_scope);
    /* not a multiple of the buffer size, so a block straddles its end */
    int total = 0;
    while (total < AUDIO_SCOPE_SIZE + 5000)
    {
        push_ramp(&g_scope, total, 1000);
        total += 1000;
    }
    ASSERT_INT_EQ((int)g_scope.head, total);

    float out[64];
    audio_scope_read(&g_scope, (uint64_t)(total - 64), out, 64);
    for (int i = 0; i < 64; i++)
        ASSERT_FLOAT_EQ(out[i], (float)(total - 64 + i));

    /* across the physical end of the buffer */
    uint64_t at = AUDIO_SCOPE_SIZE - 32;
    audio_scope_read(&g_scope, at, out, 64);
    for (int i = 0; i < 64; i++)
        ASSERT_FLOAT_EQ(out[i], (float)(at + (uint64_t)i));
}
TEST_END()

TEST_BEGIN(overwritten_and_unwritten_samples_read_as_silence)
{
    audio_scope_reset(&g_scope);
    push_ramp(&g_scope, 1, AUDIO_SCOPE_SIZE);
    push_ramp(&g_scope, 1, 10); /* samples 0..9 are gone now */

    float out[4];
    audio_scope_read(&g_scope, 0, out, 4);
    for (int i = 0; i < 4; i++)
        ASSERT_FLOAT_EQ(out[i], 0.0f);

    /* the newest is still there, what comes after it is not */
    audio_scope_read(&g_scope, g_scope.head - 2, out, 4);
    ASSERT_FLOAT_EQ(out[0], 9.0f);
    ASSERT_FLOAT_EQ(out[1], 10.0f);
    ASSERT_FLOAT_EQ(out[2], 0.0f);
    ASSERT_FLOAT_EQ(out[3], 0.0f);
}
TEST_END()

TEST_BEGIN(an_oversized_block_keeps_its_end)
{
    audio_scope_reset(&g_scope);
    int n = 2 * AUDIO_SCOPE_SIZE + 100;
    push_ramp(&g_scope, 0, n);
    ASSERT_INT_EQ((int)g_scope.head, n);

    float out[2];
    audio_scope_read(&g_scope, (uint64_t)(n - 2), out, 2);
    ASSERT_FLOAT_EQ(out[0], (float)(n - 2));
    ASSERT_FLOAT_EQ(out[1], (float)(n - 1));
}
TEST_END()

TEST_BEGIN(channels_are_averaged)
{
    audio_scope_reset(&g_scope);
    const float stereo[] = {1.0f, 0.0f, 0.5f, -0.5f, -1.0f, -1.0f};
    audio_scope_push(&g_scope, stereo, 3, 2, 44100);
    ASSERT_INT_EQ(g_scope.sample_rate, 44100);

    float out[3];
    audio_scope_read(&g_scope, 0, out, 3);
    ASSERT_FLOAT_EQ(out[0], 0.5f);
    ASSERT_FLOAT_EQ(out[1], 0.0f);
    ASSERT_FLOAT_EQ(out[2], -1.0f);
}
TEST_END()

TEST_BEGIN(minmax_finds_the_extremes_of_a_span)
{
    audio_scope_reset(&g_scope);
    const float v[] = {0.1f, -0.7f, 0.4f, 0.9f, -0.2f};
    audio_scope_push(&g_scope, v, 5, 1, 48000);

    float lo, hi;
    audio_scope_minmax(&g_scope, 0, 5, &lo, &hi);
    ASSERT_FLOAT_EQ(lo, -0.7f);
    ASSERT_FLOAT_EQ(hi, 0.9f);

    audio_scope_minmax(&g_scope, 2, 2, &lo, &hi);
    ASSERT_FLOAT_EQ(lo, 0.4f);
    ASSERT_FLOAT_EQ(hi, 0.9f);

    /* a single sample, and a request for none, both give that sample */
    audio_scope_minmax(&g_scope, 4, 0, &lo, &hi);
    ASSERT_FLOAT_EQ(lo, -0.2f);
    ASSERT_FLOAT_EQ(hi, -0.2f);

    /* beyond what was written: silence */
    audio_scope_minmax(&g_scope, 100, 8, &lo, &hi);
    ASSERT_FLOAT_EQ(lo, 0.0f);
    ASSERT_FLOAT_EQ(hi, 0.0f);
}
TEST_END()

TEST_BEGIN(nonsense_input_is_ignored)
{
    audio_scope_reset(&g_scope);
    float x = 1.0f;
    audio_scope_push(&g_scope, NULL, 4, 1, 48000);
    audio_scope_push(&g_scope, &x, 0, 1, 48000);
    audio_scope_push(&g_scope, &x, 1, 0, 48000);
    ASSERT_INT_EQ((int)g_scope.head, 0);
}
TEST_END()
