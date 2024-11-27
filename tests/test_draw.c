#include "base_test.h"

INCLUDE_BEGIN
#include "_math.h"
#include "color.h"
#include "term.h"
#include "term_draw.h"
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 src/term/term_draw.c
 src/ui/color.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(pos)
{
    str_t str = string_create();
    term_draw_pos(&str, VEC(23, 69));

    const char *expected = TESC "[70;24H";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(pos_negative)
{
    str_t str = string_create();
    term_draw_pos(&str, VEC(-102, -234));

    const char *expected = TESC "[-233;-101H";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(move_down)
{
    str_t str = string_create();
    term_draw_move(&str, VEC(0, 10));

    const char *expected = TESC "[10B";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(move_down_right)
{
    str_t str = string_create();
    term_draw_move(&str, VEC(10, 10));

    const char *expected = TESC "[10B" TESC "[10C";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(move_right)
{
    str_t str = string_create();
    term_draw_move(&str, VEC(10, 0));

    const char *expected = TESC "[10C";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(move_up_right)
{
    str_t str = string_create();
    term_draw_move(&str, VEC(10, -10));

    const char *expected = TESC "[10A" TESC "[10C";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(move_up)
{
    str_t str = string_create();
    term_draw_move(&str, VEC(0, -10));

    const char *expected = TESC "[10A";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(move_up_left)
{
    str_t str = string_create();
    term_draw_move(&str, VEC(-10, -10));

    const char *expected = TESC "[10A" TESC "[10D";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(move_left)
{
    str_t str = string_create();
    term_draw_move(&str, VEC(-10, 0));

    const char *expected = TESC "[10D";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(move_down_left)
{
    str_t str = string_create();
    term_draw_move(&str, VEC(-10, 10));

    const char *expected = TESC "[10B" TESC "[10D";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(move_center)
{
    str_t str = string_create();
    term_draw_move(&str, VEC(0, 0));

    ASSERT_INT_EQ((int)str.len, 0);
}
TEST_END()

TEST_BEGIN(color_bg_fg)
{
    str_t str = string_create();
    term_draw_color(&str, COLOR(10, 20, 30), COLOR(40, 50, 60));

    const char *expected = TESC "[48;2;10;20;30;38;2;40;50;60m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color_bg)
{
    str_t str = string_create();
    term_draw_color(&str, COLOR(10, 20, 30), COLOR_RGBA(40, 50, 60, 0));

    const char *expected = TESC "[48;2;10;20;30m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color_fg)
{
    str_t str = string_create();
    term_draw_color(&str, COLOR_RGBA(10, 20, 30, 0), COLOR(40, 50, 60));

    const char *expected = TESC "[38;2;40;50;60m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color_bg_partial_alpha)
{
    str_t str = string_create();
    term_draw_color(&str, COLOR_RGBA(10, 20, 30, 100), COLOR(40, 50, 60));

    const char *expected = TESC "[48;2;10;20;30;38;2;40;50;60m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color_fg_partial_alpha)
{
    str_t str = string_create();
    term_draw_color(&str, COLOR(10, 20, 30), COLOR_RGBA(40, 50, 60, 50));

    const char *expected = TESC "[48;2;10;20;30;38;2;40;50;60m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color_bg_fg_partial_alpha)
{
    str_t str = string_create();
    term_draw_color(&str, COLOR_RGBA(10, 20, 30, 1),
                    COLOR_RGBA(40, 50, 60, 50));

    const char *expected = TESC "[48;2;10;20;30;38;2;40;50;60m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color_bg_fg_alpha_0)
{
    str_t str = string_create();
    term_draw_color(&str, COLOR_RGBA(10, 20, 30, 0), COLOR_RGBA(40, 50, 60, 0));

    ASSERT_INT_EQ((int)str.len, 0);
}
TEST_END()

TEST_BEGIN(color256_bg_fg)
{
    term_color_mode(TERM_COLOR_256);
    str_t str = string_create();
    term_draw_color(&str, COLOR(0, 0, 100), COLOR(0, 0, 130));

    const char *expected = TESC "[48;5;17;38;5;18m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color256_bg)
{
    term_color_mode(TERM_COLOR_256);
    str_t str = string_create();
    term_draw_color(&str, COLOR(0, 0, 180), COLOR_RGBA(0, 0, 130, 0));

    const char *expected = TESC "[48;5;19m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color256_fg)
{
    term_color_mode(TERM_COLOR_256);
    str_t str = string_create();
    term_draw_color(&str, COLOR_RGBA(0, 0, 180, 0), COLOR(0, 180, 190));

    const char *expected = TESC "[38;5;37m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color256_bg_partial_alpha)
{
    term_color_mode(TERM_COLOR_256);
    str_t str = string_create();
    term_draw_color(&str, COLOR_RGBA(110, 150, 240, 100), COLOR(95, 175, 0));

    const char *expected = TESC "[48;5;69;38;5;70m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color256_fg_partial_alpha)
{
    term_color_mode(TERM_COLOR_256);
    str_t str = string_create();
    term_draw_color(&str, COLOR(128, 128, 128), COLOR_RGBA(215, 240, 245, 50));

    const char *expected = TESC "[48;5;244;38;5;195m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color256_bg_fg_partial_alpha)
{
    term_color_mode(TERM_COLOR_256);
    str_t str = string_create();
    term_draw_color(&str, COLOR_RGBA(255, 180, 180, 1),
                    COLOR_RGBA(255, 220, 100, 50));

    const char *expected = TESC "[48;5;217;38;5;221m";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(color256_bg_fg_alpha_0)
{
    term_color_mode(TERM_COLOR_256);
    str_t str = string_create();
    term_draw_color(&str, COLOR_RGBA(69, 69, 69, 0), COLOR_RGBA(96, 96, 96, 0));

    ASSERT_INT_EQ((int)str.len, 0);
}
TEST_END()

TEST_BEGIN(str)
{
    str_t str = string_create();

    term_draw_str(&str, "Hello World!", 10);

    ASSERT_INT_EQ((int)str.len, 10);
    ASSERT_STR_EQ(str.buf, "Hello Worl", str.len);
}
TEST_END()

TEST_BEGIN(str2)
{
    str_t str = string_create();

    term_draw_str(&str, "Hello \0World!", 14);

    ASSERT_INT_EQ((int)str.len, 14);
    ASSERT_STR_EQ(str.buf, "Hello \0World!", str.len);
}
TEST_END()

TEST_BEGIN(strf)
{
    str_t str = string_create();

    term_draw_strf(&str, "Hello %d %d %.1f %p %%%%%d", -100, 2012, 0.3f, NULL,
                   -6969);

    ASSERT_STR_EQ(str.buf, "Hello -100 2012 0.3 (nil) %%-6969", str.len);
}
TEST_END()

TEST_BEGIN(padding)
{
    str_t str = string_create();

    term_draw_padding(&str, 10);

    ASSERT_STR_EQ(str.buf, "          ", str.len);
}
TEST_END()

TEST_BEGIN(padding2)
{
    str_t str = string_create();

    term_draw_padding(&str, 20);

    ASSERT_STR_EQ(str.buf,
                  "          "
                  "          ",
                  str.len);
}
TEST_END()

TEST_BEGIN(hline)
{
    str_t str = string_create();

    term_draw_hline(&str, 10);

    ASSERT_STR_EQ(str.buf, TESC "[10X", str.len);
}
TEST_END()

TEST_BEGIN(hline2)
{
    str_t str = string_create();

    term_draw_hline(&str, -10);

    ASSERT_STR_EQ(str.buf, TESC "[-10X", str.len);
}
TEST_END()

TEST_BEGIN(hline3)
{
    str_t str = string_create();

    term_draw_hline(&str, 6969);

    ASSERT_STR_EQ(str.buf, TESC "[6969X", str.len);
}
TEST_END()

TEST_BEGIN(vline)
{
    str_t str = string_create();

    term_draw_vline(&str, 10, COLOR(30, 30, 30), COLOR_NONE);

    ASSERT_STR_EQ(str.buf,
                  TESC "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                       "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                       "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                       "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                       "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                       "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                       "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                       "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                       "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                       "[48;2;30;30;30m " TESC TDOWN TESC TLEFT TESC TRESET,
                  str.len);
}
TEST_END()

TEST_BEGIN(vline2)
{
    term_color_mode(TERM_COLOR_256);
    str_t str = string_create();

    term_draw_vline(&str, 10, COLOR(30, 30, 30), COLOR(30, 30, 30));

    ASSERT_STR_EQ(str.buf,
                  TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET TESC
                  "[48;5;234;38;5;234m " TESC TDOWN TESC TLEFT TESC TRESET,
                  str.len);
}
TEST_END()

TEST_BEGIN(hblockf, INIT : setlocale)
{
    str_t str = string_create();
    str_t expected = string_create();
    const wchar_t *blocks = L" ▏▎▍▌▋▊▉█";

    for (float i = 0; i < 100; i += 0.05f)
    {
        float x = i - (int)i;
        term_draw_hblockf(&str, x);

        int idx = x * 9.0f;
        string_catwch(&expected, blocks[idx]);

        ASSERT_INT_EQ((int)str.len, (int)expected.len);
        ASSERT_STR_EQ(str.buf, expected.buf, str.len);
    }
}
TEST_END()

TEST_BEGIN(vblockf, INIT : setlocale)
{
    str_t str = string_create();
    str_t expected = string_create();
    const wchar_t *blocks = L" ▁▂▃▄▅▆▇█";

    for (float i = 0; i < 100; i += 0.05f)
    {
        float x = i - (int)i;
        term_draw_vblockf(&str, x);

        int idx = x * 9.0f;
        string_catwch(&expected, blocks[idx]);

        ASSERT_INT_EQ((int)str.len, (int)expected.len);
        ASSERT_STR_EQ(str.buf, expected.buf, str.len);
    }
}
TEST_END()

TEST_BEGIN(rect)
{
    str_t str = string_create();

    term_draw_rect(&str, VEC(69, 5), COLOR(12, 23, 34), COLOR_NONE);

    char *expected =
        TESC "[48;2;12;23;34m" TESC "[69X" TESC "[B" TESC "[48;2;12;23;34m" TESC
             "[69X" TESC "[B" TESC "[48;2;12;23;34m" TESC "[69X" TESC "[B" TESC
             "[48;2;12;23;34m" TESC "[69X" TESC "[B" TESC "[48;2;12;23;34m" TESC
             "[69X" TESC "[B";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()

TEST_BEGIN(rect2)
{
    str_t str = string_create();

    term_draw_rect(&str, VEC(69, 5), COLOR_NONE, COLOR(12, 23, 34));

    char *expected =
        TESC "[38;2;12;23;34m" TESC "[69X" TESC "[B" TESC "[38;2;12;23;34m" TESC
             "[69X" TESC "[B" TESC "[38;2;12;23;34m" TESC "[69X" TESC "[B" TESC
             "[38;2;12;23;34m" TESC "[69X" TESC "[B" TESC "[38;2;12;23;34m" TESC
             "[69X" TESC "[B";
    ASSERT_STR_EQ(str.buf, expected, str.len);
}
TEST_END()
