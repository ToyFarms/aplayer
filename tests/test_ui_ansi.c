#include "base_test.h"

INCLUDE_BEGIN
#include "tui.h"
#include "ui_ansi.h"
#include <string.h>

/* tui_text.c (UTF-8 + width helpers, the only toolkit code the decoder uses)
 * also defines canvas text drawing; those need the paint module, which this
 * test has no use for. */
int tui_canvas_width(const tui_canvas *cv)
{
    (void)cv;
    return 0;
}
int tui_canvas_height(const tui_canvas *cv)
{
    (void)cv;
    return 0;
}
int tui_canvas_put(tui_canvas *cv, int x, int y, uint32_t cp, tui_pen pen)
{
    (void)cv, (void)x, (void)y, (void)cp, (void)pen;
    return 0;
}
int tui_canvas_textn(tui_canvas *cv, int x, int y, const char *s, size_t n,
                     tui_pen pen)
{
    (void)cv, (void)x, (void)y, (void)s, (void)n, (void)pen;
    return 0;
}

#define W 6
#define H 3

static void decode(const char *s, ui_cell *grid)
{
    memset(grid, 0, sizeof(ui_cell) * W * H);
    ui_ansi_decode(s, strlen(s), grid, W, H);
}

#define AT(g, x, y) ((g)[(y) * W + (x)])
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/ui
 -Isrc/tui
 -Ithirdparty/include
 src/ui/ui_ansi.c
 src/tui/tui_text.c
 thirdparty/wcwidth.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(plain_text_and_newline)
{
    ui_cell g[W * H];
    decode("ab\ncd", g);
    ASSERT_INT_EQ((int)AT(g, 0, 0).cp, 'a');
    ASSERT_INT_EQ((int)AT(g, 1, 0).cp, 'b');
    ASSERT_INT_EQ((int)AT(g, 0, 1).cp, 'c');
    ASSERT_INT_EQ((int)AT(g, 1, 1).cp, 'd');
    ASSERT_INT_EQ((int)AT(g, 2, 0).cp, 0); /* untouched */
}
TEST_END()

TEST_BEGIN(truecolor_sgr)
{
    ui_cell g[W * H];
    decode("\x1b[38;2;10;20;30m\x1b[48;2;1;2;3mX", g);
    ASSERT_INT_EQ(AT(g, 0, 0).fg.r, 10);
    ASSERT_INT_EQ(AT(g, 0, 0).fg.g, 20);
    ASSERT_INT_EQ(AT(g, 0, 0).fg.b, 30);
    ASSERT_INT_EQ(AT(g, 0, 0).bg.b, 3);
}
TEST_END()

TEST_BEGIN(palette_sgr)
{
    ui_cell g[W * H];
    decode("\x1b[38;5;196mX", g); /* xterm 196 = rgb(255,0,0) cube corner */
    ASSERT_INT_EQ(AT(g, 0, 0).fg.r, 255);
    ASSERT_INT_EQ(AT(g, 0, 0).fg.g, 0);
    decode("\x1b[38;5;240mX", g); /* grayscale ramp */
    ASSERT_INT_EQ(AT(g, 0, 0).fg.r, 8 + (240 - 232) * 10);
}
TEST_END()

TEST_BEGIN(reset_and_reverse)
{
    ui_cell g[W * H];
    decode("\x1b[7mA\x1b[0mB", g);
    ASSERT_TRUE(AT(g, 0, 0).reverse);
    ASSERT_FALSE(AT(g, 1, 0).reverse);
    ASSERT_INT_EQ(AT(g, 1, 0).fg.a, 0); /* COLOR_NONE after reset */
}
TEST_END()

TEST_BEGIN(cursor_moves)
{
    ui_cell g[W * H];
    decode("\x1b[2;3HX", g); /* row 2, col 3 (1-based) */
    ASSERT_INT_EQ((int)AT(g, 2, 1).cp, 'X');
    decode("\x1b[3C"
           "Y",
           g); /* forward 3 */
    ASSERT_INT_EQ((int)AT(g, 3, 0).cp, 'Y');
    decode("\x1b[2BZ", g); /* down 2 */
    ASSERT_INT_EQ((int)AT(g, 0, 2).cp, 'Z');
}
TEST_END()

TEST_BEGIN(out_of_bounds_is_dropped)
{
    ui_cell g[W * H];
    decode("\x1b[9;9Hx\x1b[1;1Hok", g); /* far outside, then valid */
    ASSERT_INT_EQ((int)AT(g, 0, 0).cp, 'o');
    ASSERT_INT_EQ((int)AT(g, 1, 0).cp, 'k');
    decode("0123456789", g); /* wider than the grid: no overrun */
    ASSERT_INT_EQ((int)AT(g, W - 1, 0).cp, '5');
}
TEST_END()

TEST_BEGIN(utf8_glyphs_and_unknown_sequences)
{
    ui_cell g[W * H];
    decode("\xe2\x96\x80\x1b[?25l\xe2\x96\x84", g); /* ▀ <unknown CSI> ▄ */
    ASSERT_INT_EQ((int)AT(g, 0, 0).cp, 0x2580);
    ASSERT_INT_EQ((int)AT(g, 1, 0).cp, 0x2584);
}
TEST_END()
