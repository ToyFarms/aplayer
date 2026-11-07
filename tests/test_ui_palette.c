#include "base_test.h"

INCLUDE_BEGIN
#include "ui_palette.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *picture(int w, int h)
{
    return calloc((size_t)w * (size_t)h, 3);
}

/* fills the rectangle [x0,x1) x [y0,y1) */
static void rect(uint8_t *img, int w, int x0, int y0, int x1, int y1,
                 color_t c)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
        {
            uint8_t *px = img + ((size_t)y * (size_t)w + (size_t)x) * 3;
            px[0] = c.r;
            px[1] = c.g;
            px[2] = c.b;
        }
}

static float hue_of(color_t c)
{
    return ui_color_to_oklch(c).h;
}

static float lightness(color_t c)
{
    return ui_color_to_oklch(c).L;
}

static float hue_distance(float a, float b)
{
    float d = fabsf(a - b);
    return d > 180.0f ? 360.0f - d : d;
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/ui
 src/ui/ui_palette.c
 src/term/color.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(palette_rejects_missing_picture)
{
    ui_palette p;
    uint8_t px[3] = {1, 2, 3};
    ASSERT_TRUE(!ui_palette_extract(NULL, 4, 4, &p));
    ASSERT_TRUE(!ui_palette_extract(px, 0, 4, &p));
    ASSERT_TRUE(!ui_palette_extract(px, 4, 0, &p));
    ASSERT_TRUE(!ui_palette_extract(px, 1, 1, NULL));
}
TEST_END()

TEST_BEGIN(oklch_roundtrip_and_gamut)
{
    color_t c = COLOR(200, 90, 40);
    color_t back = ui_oklch_to_color(ui_color_to_oklch(c));
    ASSERT_TRUE(abs(back.r - c.r) <= 1 && abs(back.g - c.g) <= 1 &&
                abs(back.b - c.b) <= 1);

    /* an impossible chroma is reduced, lightness and hue stay */
    ui_oklch wild = {0.85f, 0.35f, 140.0f};
    ui_oklch got = ui_color_to_oklch(ui_oklch_to_color(wild));
    ASSERT_TRUE(got.C < 0.35f);
    ASSERT_TRUE(fabsf(got.L - 0.85f) < 0.02f);
    ASSERT_TRUE(hue_distance(got.h, 140.0f) < 4.0f);
}
TEST_END()

TEST_BEGIN(palette_follows_a_solid_colour)
{
    int w = 64, h = 64;
    uint8_t *img = picture(w, h);
    color_t blue = COLOR(30, 90, 220);
    rect(img, w, 0, 0, w, h, blue);

    ui_palette p;
    ASSERT_TRUE(ui_palette_extract(img, w, h, &p));

    ASSERT_TRUE(hue_distance(hue_of(p.accent), hue_of(blue)) < 6.0f);
    ASSERT_TRUE(lightness(p.accent) > 0.7f && lightness(p.accent) < 0.9f);

    /* the page is a tinted mid-dark, not black, and text reads on it */
    ASSERT_TRUE(lightness(p.bg) > 0.15f && lightness(p.bg) < 0.40f);
    ASSERT_TRUE(ui_color_to_oklch(p.bg).C > 0.02f);
    ASSERT_TRUE(hue_distance(hue_of(p.bg), hue_of(blue)) < 12.0f);
    ASSERT_TRUE(lightness(p.text) - lightness(p.bg) > 0.55f);
    ASSERT_TRUE(lightness(p.text_dim) - lightness(p.bg) > 0.35f);
    free(img);
}
TEST_END()

TEST_BEGIN(palette_steps_are_ordered_by_lightness)
{
    ui_palette p = ui_palette_from_accent(COLOR(220, 80, 90), COLOR(230, 160, 60));
    ASSERT_TRUE(lightness(p.bg) < lightness(p.surface));
    ASSERT_TRUE(lightness(p.bg) < lightness(p.pulse));
    ASSERT_TRUE(lightness(p.bg) < lightness(p.glow));
    ASSERT_TRUE(lightness(p.hot) > lightness(p.accent));
}
TEST_END()

TEST_BEGIN(palette_prefers_vivid_over_large_dull_area)
{
    int w = 96, h = 96;
    uint8_t *img = picture(w, h);
    color_t orange = COLOR(240, 120, 10);
    rect(img, w, 0, 0, w, h, COLOR(110, 105, 100));
    rect(img, w, 30, 30, 66, 66, orange);

    ui_palette p;
    ASSERT_TRUE(ui_palette_extract(img, w, h, &p));
    ASSERT_TRUE(hue_distance(hue_of(p.accent), hue_of(orange)) < 10.0f);
    free(img);
}
TEST_END()

TEST_BEGIN(palette_second_hue_is_a_harmony_of_the_first)
{
    int w = 96, h = 96;
    uint8_t *img = picture(w, h);
    rect(img, w, 0, 0, w, h, COLOR(20, 110, 230));        /* blue, most */
    rect(img, w, 0, 0, 40, h, COLOR(240, 130, 20));       /* orange */

    ui_palette p;
    ASSERT_TRUE(ui_palette_extract(img, w, h, &p));
    float d = hue_distance(hue_of(p.accent), hue_of(p.accent2));
    /* analogous (30), split complement (150) or complement (180) only */
    ASSERT_TRUE(fabsf(d - 30.0f) < 8.0f || fabsf(d - 150.0f) < 8.0f ||
                fabsf(d - 180.0f) < 8.0f);
    ASSERT_TRUE(d > 100.0f); /* blue and orange are far apart */
    free(img);
}
TEST_END()

TEST_BEGIN(palette_single_hue_gets_an_analogous_second)
{
    int w = 64, h = 64;
    uint8_t *img = picture(w, h);
    rect(img, w, 0, 0, w, h, COLOR(200, 30, 60));

    ui_palette p;
    ASSERT_TRUE(ui_palette_extract(img, w, h, &p));
    float d = hue_distance(hue_of(p.accent), hue_of(p.accent2));
    ASSERT_TRUE(fabsf(d - 32.0f) < 8.0f);
    free(img);
}
TEST_END()

TEST_BEGIN(palette_greyscale_art_is_neutral)
{
    int w = 64, h = 64;
    uint8_t *img = picture(w, h);
    for (int y = 0; y < h; y++)
        rect(img, w, 0, y, w, y + 1, COLOR_GRAY((uint8_t)(y * 4)));

    ui_palette p;
    ASSERT_TRUE(ui_palette_extract(img, w, h, &p));
    ASSERT_TRUE(ui_color_to_oklch(p.accent).C < 0.08f);
    ASSERT_TRUE(ui_color_to_oklch(p.bg).C < 0.04f);
    free(img);
}
TEST_END()

TEST_BEGIN(palette_black_picture_is_dark_but_readable)
{
    int w = 16, h = 16;
    uint8_t *img = picture(w, h);

    ui_palette p;
    ASSERT_TRUE(ui_palette_extract(img, w, h, &p));
    ASSERT_TRUE(lightness(p.accent) > 0.7f);
    ASSERT_TRUE(lightness(p.bg) < 0.18f); /* a black cover may give a black page */
    free(img);
}
TEST_END()

TEST_BEGIN(palette_bright_art_gives_a_lighter_page)
{
    int w = 32, h = 32;
    uint8_t *dark = picture(w, h), *bright = picture(w, h);
    rect(dark, w, 0, 0, w, h, COLOR(10, 40, 110));
    rect(bright, w, 0, 0, w, h, COLOR(120, 180, 250));

    ui_palette a, b;
    ASSERT_TRUE(ui_palette_extract(dark, w, h, &a));
    ASSERT_TRUE(ui_palette_extract(bright, w, h, &b));
    ASSERT_TRUE(lightness(b.bg) > lightness(a.bg));
    free(dark);
    free(bright);
}
TEST_END()

TEST_BEGIN(palette_large_picture_is_subsampled)
{
    int w = 2000, h = 1500;
    uint8_t *img = picture(w, h);
    color_t green = COLOR(40, 200, 90);
    rect(img, w, 0, 0, w, h, green);

    ui_palette p;
    ASSERT_TRUE(ui_palette_extract(img, w, h, &p));
    ASSERT_TRUE(hue_distance(hue_of(p.accent), hue_of(green)) < 8.0f);
    free(img);
}
TEST_END()

TEST_BEGIN(palette_mix_endpoints)
{
    ui_palette a =
        ui_palette_from_accent(COLOR(220, 60, 60), COLOR(220, 160, 60));
    ui_palette b =
        ui_palette_from_accent(COLOR(60, 90, 230), COLOR(60, 200, 220));

    ui_palette lo = ui_palette_mix(&a, &b, 0.0f);
    ui_palette hi = ui_palette_mix(&a, &b, 1.0f);
    ASSERT_MEM_EQ(&lo, &a, sizeof(ui_palette));
    ASSERT_MEM_EQ(&hi, &b, sizeof(ui_palette));
}
TEST_END()
