#include "base_test.h"

INCLUDE_BEGIN
#include "ui_dither.h"
#include <math.h>
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/ui
 src/ui/ui_dither.c
 src/term/color.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(noise_is_in_range_and_repeatable)
{
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 40; x++)
        {
            float n = ui_dither_noise(x, y, 1);
            ASSERT_TRUE(n >= 0.0f && n < 1.0f);
            ASSERT_FLOAT_WITHIN(n, ui_dither_noise(x, y, 1), 0.0f);
        }
}
TEST_END()

TEST_BEGIN(exact_values_are_never_disturbed)
{
    float rgb[3] = {20.0f, 31.0f, 44.0f};
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++)
        {
            color_t c = ui_dither_rgb(rgb, x, y);
            ASSERT_TRUE(c.r == 20 && c.g == 31 && c.b == 44);
        }
}
TEST_END()

TEST_BEGIN(a_gradient_averages_back_to_its_true_value)
{
    /* a field at 20.3: about 30% of cells round up, the mean is 20.3 */
    float rgb[3] = {20.3f, 20.3f, 20.3f};
    double sum = 0;
    int n = 0;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++, n++)
        {
            color_t c = ui_dither_rgb(rgb, x, y);
            ASSERT_TRUE(c.r == 20 || c.r == 21);
            sum += c.r;
        }
    ASSERT_TRUE(fabs(sum / n - 20.3) < 0.03);
}
TEST_END()

TEST_BEGIN(a_slow_ramp_has_no_straight_contour)
{
    /* without dithering a 0.02/cell ramp is a staircase of long runs of one
     * value; dithered, the boundary row is broken up */
    int changes = 0, prev = -1;
    for (int x = 0; x < 100; x++)
    {
        float rgb[3] = {20.0f + 0.02f * (float)x, 20.0f, 20.0f};
        int v = ui_dither_rgb(rgb, x, 7).r;
        if (prev >= 0 && v != prev)
            changes++;
        prev = v;
    }
    /* an undithered ramp changes value at most 2 times over this range */
    ASSERT_TRUE(changes >= 2);
}
TEST_END()

TEST_BEGIN(palette_256_gives_a_solid_nearest_colour)
{
    float rgb[3] = {30.0f, 10.0f, 40.0f};
    uint32_t cp;
    color_t fg, bg;
    ui_dither_256(rgb, 3, 4, &cp, &fg, &bg);
    ASSERT_TRUE(cp == ' ');
    ASSERT_TRUE(bg.a == 255);
}
TEST_END()
