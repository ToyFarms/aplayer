#include "base_test.h"

INCLUDE_BEGIN
#include "ui_font.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#  include <direct.h>
#else
#  include <sys/stat.h>
#endif

/* The tests need some font on the machine; without one they pass vacuously
 * (and say so) rather than fail a build server that has none. */
static ui_font *open_font(void)
{
    ui_font *f = ui_font_open_default(NULL);
    if (f == NULL)
        fprintf(stderr, "[ui_font] no system font found, test skipped\n");
    return f;
}

static int covered(const ui_fb *fb)
{
    int n = 0;
    for (int i = 0; i < fb->w * fb->h; i++)
        n += fb->px[i * 4 + 3] >= 128;
    return n;
}

static bool exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp)
        fclose(fp);
    return fp != NULL;
}

/* Rows (of the buffer) that have ink in column range [x0, x1). */
static int rows_with_ink(const ui_fb *fb, int x0, int x1)
{
    int n = 0;
    for (int y = 0; y < fb->h; y++)
    {
        bool any = false;
        for (int x = x0; x < x1 && x < fb->w; x++)
            any |= fb->px[((size_t)y * (size_t)fb->w + (size_t)x) * 4 + 3] >= 128;
        n += any;
    }
    return n;
}

static bool copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    if (in == NULL)
        return false;
    FILE *out = fopen(to, "wb");
    if (out == NULL)
    {
        fclose(in);
        return false;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    return true;
}

static void set_env(const char *name, const char *value)
{
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value)
        setenv(name, value, 1);
    else
        unsetenv(name);
#endif
}

static void make_dir(const char *path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}
/* Number of ink pixels in column range [x0, x1). */
static int ink_in(const ui_fb *fb, int x0, int x1)
{
    int n = 0;
    for (int y = 0; y < fb->h; y++)
        for (int x = x0; x < x1 && x < fb->w; x++)
            n += fb->px[((size_t)y * (size_t)fb->w + (size_t)x) * 4 + 3] >= 128;
    return n;
}

INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/ui
 -Ithirdparty/include
 src/ui/ui_font.c
 src/ui/ui_sprite.c
 src/term/color.c
 src/struct/ds.c
 src/logger.c
 -lm
 pkg-config:freetype2
 */ CFLAGS_END

TEST_BEGIN(no_usable_file_gives_no_font)
{
    const char *paths[] = {"/definitely/not/here.ttf", "", NULL};
    ASSERT_TRUE(ui_font_open(paths, 3) == NULL);
    ASSERT_TRUE(ui_font_open(NULL, 0) == NULL);

    /* a file that is not a font */
    const char *junk[] = {"tests/test_ui_font.c"};
    ASSERT_TRUE(ui_font_open(junk, 1) == NULL);
    ui_font_close(NULL); /* must not crash */
}
TEST_END()

TEST_BEGIN(width_grows_with_text_and_size)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    float one = ui_font_width(f, "m", 1, 12.0f);
    float four = ui_font_width(f, "mmmm", 4, 12.0f);
    ASSERT_TRUE(one > 3.0f);
    ASSERT_TRUE(fabsf(four - 4.0f * one) < one * 0.1f);

    /* advances are hinted, whole pixels: each letter is off by half a pixel
     * at most */
    float big = ui_font_width(f, "mmmm", 4, 24.0f);
    ASSERT_TRUE(fabsf(big - 2.0f * four) <= 4.0f * 1.0f);
    ASSERT_TRUE(big == floorf(big) && four == floorf(four));
    ASSERT_TRUE(ui_font_width(f, "", 0, 12.0f) == 0.0f);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(metrics_are_positive_and_scale)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    float a1, d1, a2, d2;
    ui_font_metrics(f, 10.0f, &a1, &d1);
    ui_font_metrics(f, 20.0f, &a2, &d2);
    ASSERT_TRUE(a1 > 5.0f && a1 < 15.0f);
    ASSERT_TRUE(d1 > 0.5f && d1 < 5.0f);
    ASSERT_TRUE(fabsf(a2 - 2.0f * a1) <= 1.5f); /* whole pixels */
    ASSERT_TRUE(fabsf(d2 - 2.0f * d1) <= 1.5f);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(wrap_keeps_words_whole_and_lines_within_width)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    const char *text = "the quick brown fox jumps over the lazy dog";
    size_t len = strlen(text);
    float px = 12.0f;
    float max_w = ui_font_width(f, "the quick brown", 15, px) + 1.0f;

    ui_font_span sp[64];
    int n = ui_font_wrap(f, text, len, px, max_w, sp, 64);
    ASSERT_TRUE(n >= 3);

    for (int i = 0; i < n; i++)
    {
        ASSERT_TRUE(sp[i].len > 0);
        const char *s = text + sp[i].off;
        ASSERT_TRUE(s[0] != ' ' && s[sp[i].len - 1] != ' ');
        ASSERT_TRUE(ui_font_width(f, s, (size_t)sp[i].len, px) <= max_w);
        /* a break only ever falls on a space (or the end) */
        size_t end = (size_t)sp[i].off + (size_t)sp[i].len;
        ASSERT_TRUE(end == len || text[end] == ' ');
        if (i > 0)
            ASSERT_TRUE((size_t)sp[i].off > (size_t)(sp[i - 1].off + sp[i - 1].len));
    }
    ASSERT_INT_EQ(sp[0].off, 0);
    ASSERT_TRUE((size_t)(sp[n - 1].off + sp[n - 1].len) == len);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(wrap_breaks_long_words_and_always_progresses)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    const char *text = "supercalifragilisticexpialidocious";
    size_t len = strlen(text);
    ui_font_span sp[64];
    int n = ui_font_wrap(f, text, len, 12.0f, 30.0f, sp, 64);
    ASSERT_TRUE(n > 1);
    size_t total = 0;
    for (int i = 0; i < n; i++)
    {
        ASSERT_TRUE(sp[i].len > 0);
        total += (size_t)sp[i].len;
    }
    ASSERT_TRUE(total == len);

    /* narrower than any letter: still one character per line, never stuck */
    n = ui_font_wrap(f, "abc", 3, 12.0f, 0.5f, sp, 64);
    ASSERT_INT_EQ(n, 3);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(wrap_of_nothing_is_one_empty_line_and_cap_is_respected)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    ui_font_span sp[8];
    ASSERT_INT_EQ(ui_font_wrap(f, "", 0, 12.0f, 100.0f, sp, 8), 1);
    ASSERT_INT_EQ(sp[0].len, 0);
    ASSERT_INT_EQ(ui_font_wrap(f, "x", 1, 12.0f, 100.0f, sp, 0), 0);

    /* too little room: the last line takes the rest */
    const char *text = "aa bb cc dd ee ff";
    int n = ui_font_wrap(f, text, strlen(text), 12.0f, 10.0f, sp, 2);
    ASSERT_INT_EQ(n, 2);
    ASSERT_TRUE((size_t)(sp[1].off + sp[1].len) == strlen(text));
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(draw_paints_text_in_its_colour_and_only_inside_the_buffer)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    ui_fb fb = {0};
    ui_fb_resize(&fb, 20, 4); /* 40 x 16 sub-pixels */
    ui_font_draw(f, &fb, "Hello", 5, 2.0f, 11.0f, 10.0f, COLOR(200, 100, 50),
                 1.0f);
    int n = covered(&fb);
    ASSERT_TRUE(n > 20);

    bool saw = false;
    for (int i = 0; i < fb.w * fb.h; i++)
        if (fb.px[i * 4 + 3] == 255)
        {
            ASSERT_INT_EQ(fb.px[i * 4], 200);
            ASSERT_INT_EQ(fb.px[i * 4 + 1], 100);
            ASSERT_INT_EQ(fb.px[i * 4 + 2], 50);
            saw = true;
        }
    ASSERT_TRUE(saw);

    /* far outside and with no size at all: nothing happens, nothing crashes */
    ui_fb_clear(&fb);
    ui_font_draw(f, &fb, "Hello", 5, -500.0f, 11.0f, 10.0f, COLOR(1, 2, 3), 1.0f);
    ui_font_draw(f, &fb, "Hello", 5, 500.0f, 11.0f, 10.0f, COLOR(1, 2, 3), 1.0f);
    ui_font_draw(f, &fb, "Hello", 5, 2.0f, 11.0f, 0.0f, COLOR(1, 2, 3), 1.0f);
    ui_font_draw(f, &fb, "Hello", 5, 2.0f, 11.0f, 10.0f, COLOR(1, 2, 3), 0.0f);
    ASSERT_INT_EQ(covered(&fb), 0);

    ui_fb empty = {0};
    ui_font_draw(f, &empty, "Hello", 5, 2.0f, 11.0f, 10.0f, COLOR(1, 2, 3), 1.0f);
    ui_fb_free(&fb);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(bigger_text_covers_more_and_alpha_scales_coverage)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    ui_fb fb = {0};
    ui_fb_resize(&fb, 40, 12);

    ui_font_draw(f, &fb, "Wave", 4, 2.0f, 30.0f, 10.0f, COLOR(255, 255, 255), 1.0f);
    int small = covered(&fb);
    ui_fb_clear(&fb);
    ui_font_draw(f, &fb, "Wave", 4, 2.0f, 30.0f, 20.0f, COLOR(255, 255, 255), 1.0f);
    int big = covered(&fb);
    ASSERT_TRUE(big > small * 2);

    ui_fb_clear(&fb);
    ui_font_draw(f, &fb, "Wave", 4, 2.0f, 30.0f, 20.0f, COLOR(255, 255, 255), 0.3f);
    ASSERT_INT_EQ(covered(&fb), 0); /* too faint to count as covered */
    ui_fb_free(&fb);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(glyphs_sit_on_whole_pixels)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    ui_fb a = {0}, b = {0};
    ui_fb_resize(&a, 20, 4);
    ui_fb_resize(&b, 20, 4);

    /* hinted glyphs are drawn for a whole-pixel position: the same picture
     * for every x that rounds to the same pixel, one pixel over for the next */
    ui_font_draw(f, &a, "Is", 2, 6.0f, 12.0f, 12.0f, COLOR(255, 255, 255), 1.0f);
    ui_font_draw(f, &b, "Is", 2, 6.3f, 12.0f, 12.0f, COLOR(255, 255, 255), 1.0f);
    ASSERT_TRUE(memcmp(a.px, b.px, (size_t)a.w * (size_t)a.h * 4) == 0);

    ui_fb_clear(&b);
    ui_font_draw(f, &b, "Is", 2, 7.0f, 12.0f, 12.0f, COLOR(255, 255, 255), 1.0f);
    int n = 0;
    for (int y = 0; y < a.h; y++)
        for (int x = 0; x + 1 < a.w; x++)
        {
            n++;
            ASSERT_INT_EQ(a.px[((size_t)y * (size_t)a.w + (size_t)x) * 4 + 3],
                          b.px[((size_t)y * (size_t)a.w + (size_t)x + 1) * 4 + 3]);
        }
    ASSERT_TRUE(n > 0);

    /* every covered pixel is either fully covered or not at all */
    for (int i = 0; i < a.w * a.h; i++)
    {
        int al = a.px[i * 4 + 3];
        ASSERT_TRUE(al == 0 || al == 255);
    }
    ui_fb_free(&a);
    ui_fb_free(&b);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(small_text_keeps_its_counters_open)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    ui_fb fb = {0};
    ui_fb_resize(&fb, 12, 4); /* 24 x 16 */

    /* "i" has a dot with air under it, and so differs from "l": at 13 pixels
     * the blurred rendering made the two the same */
    for (int ppem = 13; ppem <= 16; ppem += 3)
    {
        ui_fb_clear(&fb);
        ui_font_draw(f, &fb, "i", 1, 4.0f, 13.0f, (float)ppem, COLOR(255, 255, 255), 1.0f);
        int i_rows = rows_with_ink(&fb, 0, fb.w);
        int span = 0, first = -1, last = -1;
        for (int y = 0; y < fb.h; y++)
            if (rows_with_ink(&(ui_fb){fb.w, 1, fb.px + (size_t)y * (size_t)fb.w * 4}, 0, fb.w))
            {
                if (first < 0)
                    first = y;
                last = y;
            }
        span = last - first + 1;
        ASSERT_TRUE(span > i_rows); /* a blank row between dot and stem */
    }

    /* an "o" is a ring: the middle of it is empty */
    ui_fb_clear(&fb);
    ui_font_draw(f, &fb, "o", 1, 4.0f, 13.0f, 13.0f, COLOR(255, 255, 255), 1.0f);
    int cy = -1, ink_row = 0;
    for (int y = 0; y < fb.h; y++)
    {
        int run = 0, edges = 0;
        bool in = false;
        for (int x = 0; x < fb.w; x++)
        {
            bool on = fb.px[((size_t)y * (size_t)fb.w + (size_t)x) * 4 + 3] >= 128;
            if (on && !in)
                edges++;
            in = on;
            run += on;
        }
        if (edges == 2)
            cy = y;
        ink_row += run > 0;
    }
    ASSERT_TRUE(cy >= 0 && ink_row >= 6);
    ui_fb_free(&fb);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(invisible_characters_take_no_room)
{
    /* with fonts that have no glyph for them these used to leave half an em */
    const char *font = "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf";
    if (!exists(font))
        font = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    const char *one[] = {font};
    ui_font *f = ui_font_open(one, 1);
    if (f == NULL)
    {
        fprintf(stderr, "[ui_font] no font to test with, skipped\n");
        return;
    }
    const char *with[] = {
        "a\xEF\xB8\x8E",     /* U+FE0E text variation selector */
        "a\xEF\xB8\x8F",     /* U+FE0F emoji variation selector */
        "a\xE2\x80\x8D",     /* zero width joiner */
        "a\xE2\x80\x8C",     /* zero width non-joiner */
        "a\xE2\x80\x8B",     /* zero width space */
        "a\xEF\xBB\xBF",     /* byte order mark */
        "a\xF3\xA0\x84\x80", /* U+E0100 ideographic variation selector */
        "a\xC2\xAD",         /* soft hyphen */
    };
    float plain = ui_font_width(f, "a", 1, 16.0f);
    for (size_t i = 0; i < sizeof(with) / sizeof(with[0]); i++)
        ASSERT_TRUE(ui_font_width(f, with[i], strlen(with[i]), 16.0f) == plain);

    /* and they do not break a word apart: the kerning of the pair survives */
    float av = ui_font_width(f, "AV", 2, 16.0f);
    float avj = ui_font_width(f, "A\xE2\x80\x8DV", 5, 16.0f);
    ASSERT_TRUE(av == avj);

    /* a character no font has still leaves a gap */
    float gap = ui_font_width(f, "a\xF3\xB0\x80\x80", 5, 16.0f); /* private use */
    ASSERT_TRUE(gap > plain + 4.0f);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(weight_axis_of_a_variable_font_is_applied)
{
    const char *var = "/usr/share/fonts/truetype/google-fonts/Lora-Variable.ttf";
    if (!exists(var))
    {
        fprintf(stderr, "[ui_font] no variable font here, test skipped\n");
        return;
    }
    const char *one[] = {var};
    ui_font *f = ui_font_open(one, 1);
    ASSERT_TRUE(f != NULL);
    ui_fb fb = {0};
    ui_fb_resize(&fb, 40, 6);

    int ink[3];
    float width[3];
    const int weights[3] = {400, 700, 0}; /* 0: the font's own default */
    for (int i = 0; i < 3; i++)
    {
        ui_font_set_weight(f, weights[i]);
        ui_fb_clear(&fb);
        ui_font_draw(f, &fb, "Hamburg", 7, 2.0f, 18.0f, 16.0f, COLOR(255, 255, 255), 1.0f);
        ink[i] = covered(&fb);
        width[i] = ui_font_width(f, "Hamburg", 7, 16.0f);
    }
    ASSERT_TRUE(ink[0] > 40);
    ASSERT_TRUE(ink[1] > ink[0] + ink[0] / 8); /* clearly heavier */
    ASSERT_INT_EQ(ink[2], ink[0]);             /* default weight of Lora: 400 */
    ASSERT_TRUE(width[1] >= width[0]);

    /* out of range is clamped, not refused */
    ui_font_set_weight(f, 5000);
    ui_fb_clear(&fb);
    ui_font_draw(f, &fb, "Hamburg", 7, 2.0f, 18.0f, 16.0f, COLOR(255, 255, 255), 1.0f);
    ASSERT_TRUE(covered(&fb) >= ink[1]);
    ui_fb_free(&fb);
    ui_font_close(f);

    /* a static font ignores it */
    const char *st = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    if (!exists(st))
        return;
    const char *two[] = {st};
    f = ui_font_open(two, 1);
    ui_fb_resize(&fb, 40, 6);
    ui_font_draw(f, &fb, "Hamburg", 7, 2.0f, 18.0f, 16.0f, COLOR(255, 255, 255), 1.0f);
    int before = covered(&fb);
    ui_font_set_weight(f, 300);
    ui_fb_clear(&fb);
    ui_font_draw(f, &fb, "Hamburg", 7, 2.0f, 18.0f, 16.0f, COLOR(255, 255, 255), 1.0f);
    ASSERT_INT_EQ(covered(&fb), before);
    ui_fb_free(&fb);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(fonts_folder_is_searched_and_unifont_comes_last)
{
    const char *a = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    const char *b = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    if (!exists(a) || !exists(b))
    {
        fprintf(stderr, "[ui_font] no DejaVu here, test skipped\n");
        return;
    }
    const char *dir = "tests/build/fonts_folder_test";
    make_dir("tests/build");
    make_dir(dir);
    char p_user[256], p_uni[256];
    snprintf(p_user, sizeof(p_user), "%s/zz_mine.ttf", dir);
    snprintf(p_uni, sizeof(p_uni), "%s/Unifont.ttf", dir); /* sorts before it */
    ASSERT_TRUE(copy_file(a, p_user));
    ASSERT_TRUE(copy_file(b, p_uni));

    set_env("APLAYER_LYRICS_FONT", NULL);
    set_env("APLAYER_FONT_DIR", dir);
    ui_font *f = ui_font_open_default(NULL);
    ASSERT_TRUE(f != NULL);
    /* a file of the folder beats the system's fonts, and Unifont - though it
     * sorts first - waits behind everything */
    ASSERT_TRUE(strcmp(ui_font_primary_path(f), p_user) == 0);
    ui_font_close(f);

    /* the file the user names beats the folder */
    f = ui_font_open_default(b);
    ASSERT_TRUE(f != NULL);
    ASSERT_TRUE(strcmp(ui_font_primary_path(f), b) == 0);
    ui_font_close(f);

    /* nothing there: back to the system's */
    set_env("APLAYER_FONT_DIR", "tests/build/no_such_folder");
    f = ui_font_open_default(NULL);
    if (f != NULL)
    {
        ASSERT_TRUE(strstr(ui_font_primary_path(f), "fonts_folder_test") == NULL);
        ui_font_close(f);
    }
    set_env("APLAYER_FONT_DIR", NULL);
    remove(p_user);
    remove(p_uni);
}
TEST_END()

TEST_BEGIN(unifont_is_exact_at_sixteen_pixels)
{
    const char *uni = "/usr/share/fonts/opentype/unifont/unifont.otf";
    if (!exists(uni))
    {
        fprintf(stderr, "[ui_font] no Unifont here, test skipped\n");
        return;
    }
    const char *one[] = {uni};
    ui_font *f = ui_font_open(one, 1);
    ASSERT_TRUE(f != NULL);

    /* half width letters are 8 wide, full width ones 16 */
    ASSERT_TRUE(ui_font_width(f, "A", 1, 16.0f) == 8.0f);
    const char *moon = "\xe6\x9c\x88"; /* U+6708 */
    const char *dream = "\xe5\xa4\xa2"; /* U+5922 */
    ASSERT_TRUE(ui_font_width(f, moon, 3, 16.0f) == 16.0f);
    ASSERT_TRUE(ui_font_width(f, dream, 3, 16.0f) == 16.0f);

    ui_fb fb = {0};
    ui_fb_resize(&fb, 12, 6);
    ui_font_draw(f, &fb, dream, 3, 2.0f, 14.0f, 16.0f, COLOR(255, 255, 255), 1.0f);
    int n = covered(&fb);
    ASSERT_TRUE(n > 40);
    /* every pixel is on or off, and the glyph stays inside its 16 x 16 box */
    for (int y = 0; y < fb.h; y++)
        for (int x = 0; x < fb.w; x++)
        {
            int al = fb.px[((size_t)y * (size_t)fb.w + (size_t)x) * 4 + 3];
            ASSERT_TRUE(al == 0 || al == 255);
            if (al)
                ASSERT_TRUE(x >= 2 && x < 18 && y >= 0 && y < 16);
        }
    ui_fb_free(&fb);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(characters_the_primary_lacks_come_from_the_fallback)
{
    const char *latin = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    const char *cjk = "/usr/share/fonts/opentype/noto/NotoSerifCJK-Bold.ttc";
    if (!exists(latin) || !exists(cjk))
    {
        fprintf(stderr, "[ui_font] no Latin + CJK pair here, test skipped\n");
        return;
    }
    const char *with[] = {latin, cjk};
    ui_font *f = ui_font_open(with, 2);
    ASSERT_TRUE(f != NULL);

    const char *han = "\xe6\x9c\x88"; /* U+6708 */
    float w = ui_font_width(f, han, strlen(han), 16.0f);
    ASSERT_TRUE(w > 12.0f); /* a full-width glyph, not the missing-glyph gap */

    ui_fb fb = {0};
    ui_fb_resize(&fb, 12, 6);
    ui_font_draw(f, &fb, han, strlen(han), 2.0f, 18.0f, 16.0f,
                 COLOR(255, 255, 255), 1.0f);
    ASSERT_TRUE(covered(&fb) > 30);
    ui_fb_free(&fb);
    ui_font_close(f);

    /* without the fallback the same character leaves a gap but no ink */
    const char *only[] = {latin};
    f = ui_font_open(only, 1);
    ui_fb_resize(&fb, 12, 6);
    ui_font_draw(f, &fb, han, strlen(han), 2.0f, 18.0f, 16.0f,
                 COLOR(255, 255, 255), 1.0f);
    ASSERT_INT_EQ(covered(&fb), 0);
    ui_fb_free(&fb);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(broken_utf8_and_control_characters_do_not_upset_it)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    const char bad[] = "ab\xff\xe6\x9c" "cd\r\n\t\x01";
    ui_font_span sp[32];
    int n = ui_font_wrap(f, bad, sizeof(bad) - 1, 12.0f, 200.0f, sp, 32);
    ASSERT_TRUE(n >= 1);

    ui_fb fb = {0};
    ui_fb_resize(&fb, 20, 4);
    ui_font_draw(f, &fb, bad, sizeof(bad) - 1, 1.0f, 12.0f, 10.0f,
                 COLOR(255, 255, 255), 1.0f);
    ASSERT_TRUE(ui_font_width(f, bad, sizeof(bad) - 1, 10.0f) > 0.0f);
    ui_fb_free(&fb);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(many_sizes_overflow_the_cache_without_corrupting_it)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    ui_fb fb = {0};
    ui_fb_resize(&fb, 40, 12);
    const char *text = "The five boxing wizards jump quickly";

    int reference = 0;
    for (int round = 0; round < 3; round++)
    {
        /* hundreds of distinct sizes: far more bitmaps than the cache holds */
        for (int q = 0; q < 400; q++)
        {
            ui_fb_clear(&fb);
            ui_font_draw(f, &fb, text, strlen(text), 1.0f, 30.0f,
                         6.0f + (float)q * 0.1f, COLOR(255, 255, 255), 1.0f);
        }
        ui_fb_clear(&fb);
        ui_font_draw(f, &fb, text, strlen(text), 1.0f, 30.0f, 12.0f,
                     COLOR(255, 255, 255), 1.0f);
        int n = covered(&fb);
        ASSERT_TRUE(n > 100);
        if (round == 0)
            reference = n;
        ASSERT_INT_EQ(n, reference); /* same picture, cached or not */
    }
    ui_fb_free(&fb);
    ui_font_close(f);
}
TEST_END()

TEST_BEGIN(a_clipped_draw_only_paints_the_columns_inside_the_clip)
{
    ui_font *f = open_font();
    if (f == NULL)
        return;
    color_t white = {255, 255, 255, 255};
    const char *s = "Karaoke";
    float px = 32.0f;
    ui_fb all = {0}, part = {0}, rest = {0};
    ui_fb_resize(&all, 40, 6);
    ui_fb_resize(&part, 40, 6);
    ui_fb_resize(&rest, 40, 6);

    ui_font_draw(f, &all, s, strlen(s), 4.0f, 28.0f, px, white, 1.0f);
    ui_font_draw_clip(f, &part, s, strlen(s), 4.0f, 28.0f, px, white, 1.0f,
                      -1e9f, 40.0f);
    ui_font_draw_clip(f, &rest, s, strlen(s), 4.0f, 28.0f, px, white, 1.0f,
                      40.0f, 1e9f);

    ASSERT_TRUE(covered(&all) > 100);
    /* nothing right of the clip in the left part, nothing left of it in the rest */
    ASSERT_INT_EQ(ink_in(&part, 40, part.w), 0);
    ASSERT_INT_EQ(ink_in(&rest, 0, 40), 0);
    ASSERT_TRUE(ink_in(&part, 0, 40) > 20);
    ASSERT_TRUE(ink_in(&rest, 40, rest.w) > 20);
    /* where they meet, the two together are the whole text, pixel for pixel */
    ASSERT_INT_EQ(covered(&part) + covered(&rest), covered(&all));
    for (int i = 0; i < all.w * all.h * 4; i++)
    {
        int both = part.px[i] > rest.px[i] ? part.px[i] : rest.px[i];
        ASSERT_INT_EQ(both, all.px[i]);
    }
    /* an empty clip draws nothing */
    ui_fb none = {0};
    ui_fb_resize(&none, 40, 6);
    ui_font_draw_clip(f, &none, s, strlen(s), 4.0f, 28.0f, px, white, 1.0f,
                      30.0f, 30.0f);
    ASSERT_INT_EQ(covered(&none), 0);
    ui_fb_free(&all);
    ui_fb_free(&part);
    ui_fb_free(&rest);
    ui_fb_free(&none);
    ui_font_close(f);
}
TEST_END()
