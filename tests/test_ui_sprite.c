#include "base_test.h"

INCLUDE_BEGIN
#include "ui_sprite.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* a solid w*h picture */
static uint8_t *solid(int w, int h, color_t c)
{
    uint8_t *img = malloc((size_t)w * (size_t)h * 3);
    for (int i = 0; i < w * h; i++)
    {
        img[i * 3] = c.r;
        img[i * 3 + 1] = c.g;
        img[i * 3 + 2] = c.b;
    }
    return img;
}

/* left half red, right half blue */
static uint8_t *two_halves(int w, int h)
{
    uint8_t *img = solid(w, h, COLOR(255, 0, 0));
    for (int y = 0; y < h; y++)
        for (int x = w / 2; x < w; x++)
        {
            uint8_t *p = img + ((size_t)y * (size_t)w + (size_t)x) * 3;
            p[0] = 0;
            p[2] = 255;
        }
    return img;
}

/* the few block shapes the tests look for; anything else is a braille dot */
static uint32_t test_blocks(uint8_t mask)
{
    switch (mask)
    {
    case 0x0F: return 0x2580; /* upper half */
    case 0xF0: return 0x2584; /* lower half */
    case 0x55: return 0x258C; /* left half */
    case 0xAA: return 0x2590; /* right half */
    default: return 0x2800u + mask;
    }
}

static uint32_t test_braille(uint8_t mask)
{
    return 0x2800u + mask;
}

static ui_sprite card(const uint8_t *rgb, int w, int h, float cx, float cy,
                      float size)
{
    return (ui_sprite){.rgb = rgb, .w = w, .h = h, .cx = cx, .cy = cy,
                       .width = size, .height = size, .alpha = 1.0f};
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/ui
 src/ui/ui_sprite.c
 src/term/color.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(fb_resize_is_cleared_and_sized_in_subpixels)
{
    ui_fb fb = {0};
    ui_fb_resize(&fb, 5, 3);
    ASSERT_INT_EQ(fb.w, 5 * UI_SUBCELL_W);
    ASSERT_INT_EQ(fb.h, 3 * UI_SUBCELL_H);
    for (int i = 0; i < fb.w * fb.h * 4; i++)
        ASSERT_INT_EQ(fb.px[i], 0);
    ui_fb_free(&fb);
    ASSERT_TRUE(fb.px == NULL);
}
TEST_END()

TEST_BEGIN(sprite_covers_exactly_its_rectangle)
{
    ui_fb fb = {0};
    ui_fb_resize(&fb, 8, 4); /* 16 x 16 sub-pixels */
    uint8_t *img = solid(4, 4, COLOR(200, 100, 50));
    ui_sprite s = card(img, 4, 4, 8.0f, 8.0f, 8.0f); /* x, y in [4, 12) */
    ui_fb_draw(&fb, &s);

    for (int y = 0; y < fb.h; y++)
        for (int x = 0; x < fb.w; x++)
        {
            size_t at = ((size_t)y * (size_t)fb.w + (size_t)x) * 4;
            const uint8_t *p = fb.px + at;
            bool inside = x >= 4 && x < 12 && y >= 4 && y < 12;
            ASSERT_INT_EQ(p[3], inside ? 255 : 0);
            if (inside)
            {
                ASSERT_INT_EQ(p[0], 200);
                ASSERT_INT_EQ(p[1], 100);
                ASSERT_INT_EQ(p[2], 50);
            }
        }
    free(img);
    ui_fb_free(&fb);
}
TEST_END()

TEST_BEGIN(sprite_clips_at_the_framebuffer_edge)
{
    ui_fb fb = {0};
    ui_fb_resize(&fb, 4, 2);
    uint8_t *img = solid(2, 2, COLOR(10, 20, 30));
    ui_sprite s = card(img, 2, 2, -3.0f, 100.0f, 20.0f); /* mostly outside */
    ui_fb_draw(&fb, &s);
    s.cx = 4.0f;
    s.cy = 4.0f;
    s.width = s.height = 1000.0f; /* covers everything */
    ui_fb_draw(&fb, &s);
    for (int i = 0; i < fb.w * fb.h; i++)
        ASSERT_INT_EQ(fb.px[i * 4 + 3], 255);
    free(img);
    ui_fb_free(&fb);
}
TEST_END()

TEST_BEGIN(sprite_degenerate_input_draws_nothing)
{
    ui_fb fb = {0};
    ui_fb_resize(&fb, 4, 2);
    uint8_t *img = solid(2, 2, COLOR(255, 255, 255));
    ui_sprite s = card(img, 2, 2, 4.0f, 4.0f, 6.0f);

    ui_sprite z = s;
    z.alpha = 0.0f;
    ui_fb_draw(&fb, &z);
    z = s;
    z.width = 0.0f;
    ui_fb_draw(&fb, &z);
    z = s;
    z.rgb = NULL;
    ui_fb_draw(&fb, &z);
    z = s;
    z.w = 0;
    ui_fb_draw(&fb, &z);

    ui_fb empty = {0};
    ui_fb_draw(&empty, &s); /* no framebuffer: must not crash */

    for (int i = 0; i < fb.w * fb.h; i++)
        ASSERT_INT_EQ(fb.px[i * 4 + 3], 0);
    free(img);
    ui_fb_free(&fb);
}
TEST_END()

TEST_BEGIN(sprite_rotation_moves_corners)
{
    ui_sprite s = card(NULL, 2, 2, 20.0f, 20.0f, 20.0f);
    ASSERT_TRUE(ui_sprite_contains(&s, 29.0f, 29.0f)); /* near the corner */

    s.angle = (float)M_PI / 4.0f; /* a diamond now */
    ASSERT_TRUE(!ui_sprite_contains(&s, 29.0f, 29.0f));
    ASSERT_TRUE(ui_sprite_contains(&s, 20.0f, 20.0f));
    ASSERT_TRUE(ui_sprite_contains(&s, 20.0f, 20.0f + 13.0f)); /* new tip */
}
TEST_END()

TEST_BEGIN(sprite_rounded_corners_are_cut)
{
    ui_sprite s = card(NULL, 2, 2, 20.0f, 20.0f, 20.0f);
    ASSERT_TRUE(ui_sprite_contains(&s, 10.5f, 10.5f));
    s.radius = 6.0f;
    ASSERT_TRUE(!ui_sprite_contains(&s, 10.5f, 10.5f));
    ASSERT_TRUE(ui_sprite_contains(&s, 20.0f, 10.5f)); /* edges stay */
}
TEST_END()

TEST_BEGIN(sprite_over_blends_by_alpha)
{
    ui_fb fb = {0};
    ui_fb_resize(&fb, 2, 1);
    uint8_t *red = solid(2, 2, COLOR(255, 0, 0));
    uint8_t *blue = solid(2, 2, COLOR(0, 0, 255));
    ui_sprite a = card(red, 2, 2, 2.0f, 2.0f, 100.0f);
    ui_sprite b = card(blue, 2, 2, 2.0f, 2.0f, 100.0f);
    b.alpha = 0.5f;
    ui_fb_draw(&fb, &a);
    ui_fb_draw(&fb, &b);

    const uint8_t *p = fb.px;
    ASSERT_INT_EQ(p[3], 255);
    ASSERT_TRUE(abs(p[0] - 128) <= 1);
    ASSERT_TRUE(abs(p[2] - 128) <= 1);

    /* half-transparent over nothing stays half transparent */
    ui_fb_clear(&fb);
    ui_fb_draw(&fb, &b);
    ASSERT_TRUE(abs(fb.px[3] - 128) <= 1);
    free(red);
    free(blue);
    ui_fb_free(&fb);
}
TEST_END()

TEST_BEGIN(sprite_shade_fades_towards_fog)
{
    ui_fb fb = {0};
    ui_fb_resize(&fb, 1, 1);
    uint8_t *img = solid(2, 2, COLOR(200, 200, 200));
    ui_sprite s = card(img, 2, 2, 1.0f, 2.0f, 100.0f);
    s.fog = COLOR(0, 0, 0);
    s.shade = 0.5f;
    ui_fb_draw(&fb, &s);
    ASSERT_TRUE(abs(fb.px[0] - 100) <= 1);
    free(img);
    ui_fb_free(&fb);
}
TEST_END()

TEST_BEGIN(cells_untouched_where_nothing_is_drawn)
{
    ui_fb fb = {0};
    ui_fb_resize(&fb, 3, 2);
    ui_cell grid[6];
    memset(grid, 0xAB, sizeof(grid));
    ui_fb_to_cells(&fb, grid, UI_FB_BLOCKS, test_blocks);
    for (int i = 0; i < 6; i++)
        ASSERT_INT_EQ((int)grid[i].cp, 0);
    ui_fb_free(&fb);
}
TEST_END()

TEST_BEGIN(cells_flat_colour_is_a_plain_background)
{
    ui_fb fb = {0};
    ui_fb_resize(&fb, 2, 2);
    uint8_t *img = solid(2, 2, COLOR(40, 160, 220));
    ui_sprite s = card(img, 2, 2, 2.0f, 4.0f, 100.0f);
    ui_fb_draw(&fb, &s);

    ui_cell grid[4];
    ui_fb_to_cells(&fb, grid, UI_FB_BLOCKS, test_blocks);
    for (int i = 0; i < 4; i++)
    {
        ASSERT_INT_EQ((int)grid[i].cp, ' ');
        ASSERT_INT_EQ(grid[i].bg.r, 40);
        ASSERT_INT_EQ(grid[i].bg.g, 160);
        ASSERT_INT_EQ(grid[i].bg.b, 220);
        ASSERT_INT_EQ(grid[i].bg.a, 255);
    }
    free(img);
    ui_fb_free(&fb);
}
TEST_END()

TEST_BEGIN(cells_vertical_edge_becomes_a_half_block)
{
    /* one cell, left half red, right half blue: a left/right split */
    ui_fb fb = {0};
    ui_fb_resize(&fb, 1, 1);
    uint8_t *img = two_halves(2, 2);
    ui_sprite s = card(img, 2, 2, 1.0f, 2.0f, 100.0f);
    s.width = 2.0f; /* exactly the cell, so sampling lands on the halves */
    s.height = 4.0f;
    ui_fb_draw(&fb, &s);

    ui_cell cell;
    ui_fb_to_cells(&fb, &cell, UI_FB_BLOCKS, test_blocks);
    ASSERT_TRUE(cell.cp == 0x258C || cell.cp == 0x2590); /* ▌ or ▐ */
    ASSERT_TRUE(cell.fg.a == 255 && cell.bg.a == 255);
    bool red_left = cell.cp == 0x258C ? cell.fg.r > 200 : cell.bg.r > 200;
    ASSERT_TRUE(red_left);
    free(img);
    ui_fb_free(&fb);
}
TEST_END()

TEST_BEGIN(cells_partial_cover_keeps_background_transparent)
{
    /* the card ends in the middle of the cell: its right half is empty */
    ui_fb fb = {0};
    ui_fb_resize(&fb, 2, 1); /* 4 x 4 sub-pixels */
    uint8_t *img = solid(2, 2, COLOR(255, 128, 0));
    ui_sprite s = card(img, 2, 2, 1.5f, 2.0f, 100.0f);
    s.width = 3.0f; /* x in [0, 3) */
    s.height = 100.0f;
    ui_fb_draw(&fb, &s);

    ui_cell grid[2];
    ui_fb_to_cells(&fb, grid, UI_FB_BLOCKS, test_blocks);
    ASSERT_INT_EQ(grid[0].bg.a, 255); /* first cell is fully covered */
    ASSERT_TRUE(grid[1].cp != 0);     /* second one only partly */
    ASSERT_INT_EQ(grid[1].bg.a, 0);   /* so the page shows through */
    ASSERT_INT_EQ(grid[1].fg.r, 255);
    ASSERT_TRUE(grid[1].cp == 0x258C); /* ▌: left column only */
    free(img);
    ui_fb_free(&fb);
}
TEST_END()

TEST_BEGIN(cells_braille_mode_uses_braille_glyphs)
{
    ui_fb fb = {0};
    ui_fb_resize(&fb, 1, 1);
    uint8_t *img = two_halves(2, 2);
    ui_sprite s = card(img, 2, 2, 1.0f, 2.0f, 100.0f);
    s.width = 2.0f;
    s.height = 4.0f;
    ui_fb_draw(&fb, &s);

    ui_cell cell;
    ui_fb_to_cells(&fb, &cell, UI_FB_BRAILLE, test_braille);
    ASSERT_TRUE(cell.cp >= 0x2800 && cell.cp <= 0x28FF);
    free(img);
    ui_fb_free(&fb);
}
TEST_END()
