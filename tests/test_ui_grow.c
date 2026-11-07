#include "base_test.h"

INCLUDE_BEGIN
#include "ui_grow.h"
#include <math.h>

/* runs the animation for `secs` in 1/60 s frames */
static void run(ui_grow *g, int focus, float secs, float dur)
{
    int frames = (int)lroundf(secs * 60.0f);
    for (int i = 0; i < frames; i++)
        ui_grow_step(g, focus, 1.0f / 60.0f, dur);
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/ui
 src/ui/ui_grow.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(starts_small_and_untouched_lines_stay_small)
{
    ui_grow g = {0};
    ASSERT_TRUE(ui_grow_get(&g, 0) == 0.0f);
    ASSERT_TRUE(ui_grow_get(&g, 7) == 0.0f);
    ASSERT_FALSE(ui_grow_busy(&g, -1));
}
TEST_END()

TEST_BEGIN(focus_grows_to_full_size_in_the_given_time)
{
    ui_grow g = {0};
    ui_grow_step(&g, 3, 0.0f, 0.4f);
    ASSERT_TRUE(ui_grow_get(&g, 3) == 0.0f); /* a new slot starts small */
    ASSERT_TRUE(ui_grow_busy(&g, 3));

    run(&g, 3, 0.2f, 0.4f);
    float half = ui_grow_get(&g, 3);
    ASSERT_TRUE(half > 0.4f && half < 0.6f);

    run(&g, 3, 0.25f, 0.4f);
    ASSERT_TRUE(ui_grow_get(&g, 3) == 1.0f);
    ASSERT_FALSE(ui_grow_busy(&g, 3));
}
TEST_END()

TEST_BEGIN(growth_is_monotonic)
{
    ui_grow g = {0};
    float last = -1.0f;
    for (int i = 0; i < 90; i++)
    {
        ui_grow_step(&g, 2, 1.0f / 60.0f, 0.5f);
        float e = ui_grow_get(&g, 2);
        ASSERT_TRUE(e >= last);
        last = e;
    }
    ASSERT_TRUE(last == 1.0f);
}
TEST_END()

TEST_BEGIN(old_line_shrinks_as_the_new_one_grows_and_their_sizes_add_up)
{
    ui_grow g = {0};
    run(&g, 4, 1.0f, 0.4f); /* line 4 is fully big */
    ASSERT_TRUE(ui_grow_get(&g, 4) == 1.0f);

    for (int i = 0; i < 24; i++)
    {
        ui_grow_step(&g, 5, 1.0f / 60.0f, 0.4f);
        float sum = ui_grow_get(&g, 4) + ui_grow_get(&g, 5);
        ASSERT_TRUE(fabsf(sum - 1.0f) < 1e-4f);
    }
    ASSERT_TRUE(ui_grow_get(&g, 4) == 0.0f);
    ASSERT_TRUE(ui_grow_get(&g, 5) == 1.0f);
}
TEST_END()

TEST_BEGIN(finished_lines_leave_the_set)
{
    ui_grow g = {0};
    run(&g, 1, 1.0f, 0.3f);
    run(&g, 2, 1.0f, 0.3f);
    ASSERT_INT_EQ(g.n, 1);
    ASSERT_INT_EQ(g.s[0].line, 2);

    run(&g, -1, 1.0f, 0.3f); /* nothing is big any more */
    ASSERT_INT_EQ(g.n, 0);
    ASSERT_FALSE(ui_grow_busy(&g, -1));
}
TEST_END()

TEST_BEGIN(a_line_that_never_finished_growing_just_goes_back)
{
    ui_grow g = {0};
    run(&g, 1, 0.1f, 0.4f); /* a quarter of the way */
    float before = ui_grow_get(&g, 1);
    ASSERT_TRUE(before > 0.0f && before < 0.5f);

    run(&g, 2, 0.1f, 0.4f); /* focus moves on: line 1 shrinks from there */
    ASSERT_TRUE(ui_grow_get(&g, 1) < before);
}
TEST_END()

TEST_BEGIN(zero_duration_is_instant)
{
    ui_grow g = {0};
    ui_grow_step(&g, 6, 0.016f, 0.0f);
    ASSERT_TRUE(ui_grow_get(&g, 6) == 1.0f);
    ui_grow_step(&g, 7, 0.016f, 0.0f);
    ASSERT_TRUE(ui_grow_get(&g, 6) == 0.0f);
    ASSERT_TRUE(ui_grow_get(&g, 7) == 1.0f);
    ASSERT_INT_EQ(g.n, 1);
}
TEST_END()

TEST_BEGIN(a_burst_of_seeks_never_overflows_the_set)
{
    ui_grow g = {0};
    for (int line = 0; line < 40; line++)
    {
        ui_grow_step(&g, line, 0.01f, 1.0f);
        ASSERT_TRUE(g.n <= UI_GROW_MAX);
        /* the line asked for is always in the set */
        ASSERT_TRUE(ui_grow_get(&g, line) >= 0.0f);
        bool found = false;
        for (int i = 0; i < g.n; i++)
            found |= g.s[i].line == line;
        ASSERT_TRUE(found);
    }
}
TEST_END()

TEST_BEGIN(negative_dt_does_not_run_it_backwards)
{
    ui_grow g = {0};
    run(&g, 1, 0.1f, 0.4f);
    float e = ui_grow_get(&g, 1);
    ui_grow_step(&g, 1, -5.0f, 0.4f);
    ASSERT_TRUE(ui_grow_get(&g, 1) == e);
}
TEST_END()
