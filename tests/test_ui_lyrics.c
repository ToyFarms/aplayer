#include "base_test.h"

INCLUDE_BEGIN
/* The widget itself, not a model of it: ui_lyrics.c is included, and the few
 * toolkit and application functions it calls are replaced below by fakes that
 * record what it paints. Frames are then run one by one, as the main loop
 * would, while the "music" moves on from line to line. */
#include "ui_lyrics.c"

#include <stdio.h>

static int g_node; /* what tui__open() "creates" */
static int g_canvas_w, g_canvas_h;
static tui_rect g_rect;
static audio_source g_src;
static metadata_t g_meta;
static app_instance g_app;
static audio_ctx g_audio;
static ui_font *g_font;
static ui_ctx g_u;

typedef struct painted
{
    int x, y;
    char text[32];
} painted;
static painted g_rows[512];
static int g_nrows;

audio_source *app_current_source(app_instance *app)
{
    (void)app;
    return &g_src;
}

static metadata_t *fake_get_metadata(audio_source *s)
{
    (void)s;
    return &g_meta;
}

tui_node *tui__open(tui_ctx *ctx, tui_kind kind, const tui_attr *attr)
{
    (void)ctx, (void)kind, (void)attr;
    return (tui_node *)&g_node;
}
void tui__close(tui_ctx *ctx) { (void)ctx; }
tui_attr tui_get_attr(const tui_node *n)
{
    (void)n;
    return (tui_attr){0};
}
void tui_set_attr(tui_node *n, const tui_attr *a) { (void)n, (void)a; }
static int g_invalidations;
void tui_invalidate(tui_node *n)
{
    (void)n;
    g_invalidations++;
}
tui_rect tui_get_content_rect(const tui_node *n)
{
    (void)n;
    return g_rect;
}

/* the lyrics in the tests are ASCII: one byte, one column */
size_t tui_utf8_decode(const char *s, size_t len, uint32_t *cp)
{
    if (len == 0)
        return 0;
    *cp = (unsigned char)s[0];
    return 1;
}
int tui_cp_width(uint32_t cp)
{
    (void)cp;
    return 1;
}
int tui_text_width(const char *s, size_t len)
{
    (void)s;
    return (int)len;
}

int tui_canvas_width(const tui_canvas *c)
{
    (void)c;
    return g_canvas_w;
}
int tui_canvas_height(const tui_canvas *c)
{
    (void)c;
    return g_canvas_h;
}
int tui_canvas_put(tui_canvas *c, int x, int y, uint32_t cp, tui_pen pen)
{
    (void)c, (void)x, (void)y, (void)cp, (void)pen;
    return 1;
}
void tui_canvas_fill(tui_canvas *c, tui_rect r, uint32_t cp, tui_pen pen)
{
    (void)c, (void)r, (void)cp, (void)pen;
}
int tui_canvas_text(tui_canvas *c, int x, int y, const char *s, tui_pen pen)
{
    (void)c, (void)x, (void)y, (void)s, (void)pen;
    return 0;
}
int tui_canvas_textn(tui_canvas *c, int x, int y, const char *s, size_t len,
                     tui_pen pen)
{
    (void)c, (void)pen;
    if (g_nrows < 512)
    {
        painted *p = &g_rows[g_nrows++];
        p->x = x;
        p->y = y;
        size_t n = len < sizeof(p->text) - 1 ? len : sizeof(p->text) - 1;
        memcpy(p->text, s, n);
        p->text[n] = '\0';
    }
    return (int)len;
}

wchar_t image_mask_to_glyph(uint8_t mask) { return (wchar_t)(0x2580 + mask); }
wchar_t image_mask_to_braille(uint8_t mask) { return (wchar_t)(0x2800 + mask); }
float ui_ambience_pulse(const ui_ambience *a, const ui_ambience_params *p)
{
    (void)a, (void)p;
    return 0.0f;
}
void ui_action_seek_to(ui_ctx *u, float f) { (void)u, (void)f; }
ui_font *ui_ctx_font(ui_ctx *u)
{
    (void)u;
    return g_font;
}

/* ---- word timing, faked ------------------------------------------------- */

static int g_kfocus = -1;
static int g_kcalls;     /* get_karaoke() calls */
static int g_kline_for = -1; /* the line g_kline is the timing of */
static kara_syl g_ksyl[4];
static kara_line g_kline;
static int g_kdummy;

static karaoke *fake_get_karaoke(audio_source *s)
{
    (void)s;
    g_kcalls++;
    return (karaoke *)&g_kdummy;
}
void kara_focus(karaoke *k, int line)
{
    (void)k;
    g_kfocus = line;
}
const kara_line *kara_line_get(karaoke *k, int line)
{
    (void)k;
    return line == g_kline_for ? &g_kline : NULL;
}

/* ---- the scene ---------------------------------------------------------- */

enum
{
    L_ABOVE,
    L_SHORT, /* one row, big or small */
    L_LONG,  /* one small row, three big ones */
    L_BELOW1,
    L_BELOW2,
    L_BELOW3,
    L_LAST_SHORT, /* one row again */
    L_COUNT
};

static const char *const TEXTS[L_COUNT] = {
    "Above the rest",
    "Short",
    "A much longer line which has to wrap over three big rows of letters",
    "Below one",
    "Below two",
    "Below three",
    "Shorter again",
};

static ui_lyrics *scene(int w, int h)
{
    static synced_lyric_line_t lines[L_COUNT];
    for (int i = 0; i < L_COUNT; i++)
    {
        lines[i].text = (char *)TEXTS[i];
        lines[i].start_ms = i * 3000;
    }
    memset(&g_meta, 0, sizeof(g_meta));
    g_meta.lyrics_synced = lines;
    g_meta.n_lyrics_synced = L_COUNT;

    memset(&g_src, 0, sizeof(g_src));
    g_src.get_metadata = fake_get_metadata;
    g_src.duration = 60 * 1000000LL;
    memset(&g_audio, 0, sizeof(g_audio));
    memset(&g_app, 0, sizeof(g_app));
    g_app.audio = &g_audio;
    g_app.track_serial = 1;

    memset(&g_u, 0, sizeof(g_u));
    g_u.app = &g_app;
    g_u.opt = ui_options_default();
    g_rect = (tui_rect){0, 0, w, h};
    g_canvas_w = w;
    g_canvas_h = h;

    if (g_font == NULL)
        g_font = ui_font_open_default(NULL);
    ui_font_set_weight(g_font, 700);
    return ui_lyrics_new(&g_u, 0);
}

static void frame(ui_lyrics *l, double dt)
{
    lyrics_update((tui_node *)&g_node, dt, l);
    g_nrows = 0;
    lyrics_paint((tui_node *)&g_node, (tui_canvas *)&g_node, l);
}

static void run(ui_lyrics *l, int ms)
{
    for (int t = 0; t < ms; t += 16)
        frame(l, 0.016);
}

/* row (on screen, painted) of the small line that starts with `prefix`, -1 if
 * it is not painted */
static int painted_row(const char *prefix)
{
    for (int i = 0; i < g_nrows; i++)
        if (strncmp(g_rows[i].text, prefix, strlen(prefix)) == 0)
            return g_rows[i].y;
    return -1;
}

/* where a line is on screen, continuously */
static float screen_y(ui_lyrics *l, int line)
{
    return line_y(l, line) - l->pos;
}

typedef struct track
{
    float y[600];
    int n;
} track;

/* Is the sequence one-way? Returns the largest step against its overall
 * direction (0 for a clean motion). */
static float backslide(const track *t)
{
    float total = t->y[t->n - 1] - t->y[0];
    float dir = total >= 0.0f ? 1.0f : -1.0f;
    float worst = 0.0f;
    for (int i = 1; i < t->n; i++)
    {
        float against = -(t->y[i] - t->y[i - 1]) * dir;
        if (against > worst)
            worst = against;
    }
    return worst;
}

/* Plays from `from` to `to`, and tracks where a line above the two, and the
 * lines below them, are on screen on every frame of the change. */
typedef struct motion
{
    track above, below1, below3, row_below1, row_above;
    float above_travel, below_travel;
} motion;

static void make_move(ui_lyrics *l, int from, int to, motion *m)
{
    memset(m, 0, sizeof(*m));
    g_src.timestamp = (int64_t)from * 3000 * 1000;
    run(l, 1500); /* `from` is read, grown, at rest */
    ASSERT_INT_EQ(l->current, from);
    ASSERT_TRUE(!ui_grow_busy(&l->grow, from));

    g_src.timestamp = (int64_t)to * 3000 * 1000;
    int below1 = MATH_MIN(to + 1, L_COUNT - 1), below3 = MATH_MIN(to + 3, L_COUNT - 1);
    for (int f = 0; f < 120; f++) /* ~1.9 s: the transition and its tail */
    {
        frame(l, 0.016);
        track *tr[3] = {&m->above, &m->below1, &m->below3};
        float v[3] = {screen_y(l, L_ABOVE), screen_y(l, below1),
                      screen_y(l, below3)};
        for (int k = 0; k < 3; k++)
            tr[k]->y[tr[k]->n++] = v[k];
        m->row_below1.y[m->row_below1.n++] = (float)painted_row(TEXTS[below1]);
        m->row_above.y[m->row_above.n++] = (float)painted_row(TEXTS[L_ABOVE]);
    }
    if (getenv("LYRICS_TRACE")) /* LYRICS_TRACE=1 python3 test.py ...: the rows */
    {
        fprintf(stderr, "line %d -> %d, painted row of the line below / above:\n", from, to);
        for (int f = 0; f < m->below1.n; f += 3)
            fprintf(stderr, "  f%03d  below %3.0f (%.2f)  above %3.0f (%.2f)\n", f,
                    m->row_below1.y[f], m->below1.y[f], m->row_above.y[f],
                    m->above.y[f]);
    }
}
/* "Short" is two units, "Sh" sung 3100..3500 ms and "ort" 3500..4000. */
static void give_timing(int line)
{
    g_ksyl[0] = (kara_syl){.start_ms = 3100, .end_ms = 3500, .off = 0, .len = 2,
                           .nsub = 1, .word_start = true, .conf = 1.0f};
    g_ksyl[1] = (kara_syl){.start_ms = 3500, .end_ms = 4000, .off = 2, .len = 3,
                           .nsub = 1, .conf = 1.0f};
    g_kline = (kara_line){.n = 2, .syl = g_ksyl, .conf = 1.0f};
    g_kline_for = line;
    g_kfocus = -1;
    g_kcalls = 0;
}

/* the pieces of "Short" painted this frame, in order (what is painted of
 * it is always a substring of it) */
static int pieces(char out[8][32], int xs[8], int row_from, int row_to)
{
    int n = 0;
    for (int i = 0; i < g_nrows && n < 8; i++)
        if (g_rows[i].y >= row_from && g_rows[i].y <= row_to &&
            g_rows[i].text[0] != '\0' && strstr("Short", g_rows[i].text) != NULL)
        {
            strcpy(out[n], g_rows[i].text);
            xs[n++] = g_rows[i].x;
        }
    return n;
}

static void at(ui_lyrics *l, int64_t ms)
{
    g_src.timestamp = ms * 1000;
    run(l, 160); /* long enough for the line to be the current one */
}

/* ink of the big line's framebuffer: the colour (red channel, the theme is
 * grey) of the leftmost and the rightmost solid pixel */
static bool fb_ends(const ui_lyrics *l, int *left_x, int *left_r, int *right_x,
                    int *right_r)
{
    *left_x = -1;
    *right_x = -1;
    for (int x = 0; x < l->fb.w; x++)
        for (int y = 0; y < l->fb.h; y++)
        {
            const uint8_t *d = l->fb.px + ((size_t)y * (size_t)l->fb.w + (size_t)x) * 4;
            if (d[3] < 250)
                continue;
            if (*left_x < 0)
            {
                *left_x = x;
                *left_r = d[0];
            }
            *right_x = x;
            *right_r = d[0];
        }
    return *left_x >= 0;
}

#define NEED_FONT()                                                            \
    do                                                                         \
    {                                                                          \
        g_font = ui_font_open_default(NULL);                                   \
        if (g_font == NULL)                                                    \
        {                                                                      \
            fprintf(stderr, "[ui_lyrics] no font on this machine, skipped\n"); \
            return;                                                            \
        }                                                                      \
    } while (0)
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/ui
 -Isrc/ui/widgets
 -Isrc/tui
 -Ithirdparty/include
 -Ibuild/_deps/portaudio-src/include
 src/ui/ui_font.c
 src/ui/ui_sprite.c
 src/ui/ui_grow.c
 src/ui/ui_options.c
 src/term/color.c
 src/struct/ds.c
 src/logger.c
 -lm
 pkg-config:freetype2
 */ CFLAGS_END

TEST_BEGIN(a_short_line_then_a_three_row_line_moves_the_page_one_way)
{
    NEED_FONT();
    ui_lyrics *l = scene(100, 40);
    motion m;
    make_move(l, L_SHORT, L_LONG, &m);

    /* the big form of the long line really is three rows of text, taller than
     * the short one: this is the case where the page used to jump */
    const big *bl = big_for(l, L_LONG);
    ASSERT_TRUE(bl != NULL);
    ASSERT_INT_EQ(bl->n, 3);
    const big *bs = big_for(l, L_SHORT);
    ASSERT_TRUE(bs != NULL && bs->block < bl->block);

    /* every line moves from where it was to where it ends up, and never
     * backwards on the way: up, then down, was the complaint */
    ASSERT_TRUE(backslide(&m.below1) < 0.01f);
    ASSERT_TRUE(backslide(&m.below3) < 0.01f);
    ASSERT_TRUE(backslide(&m.above) < 0.01f);
    /* and as the rows the terminal text is actually painted on */
    ASSERT_TRUE(backslide(&m.row_below1) < 0.5f);
    ASSERT_TRUE(backslide(&m.row_above) < 0.5f);
    /* it did motion: this is not passing by standing still */
    ASSERT_TRUE(fabsf(m.below1.y[m.below1.n - 1] - m.below1.y[0]) > 2.0f);
}
TEST_END()

TEST_BEGIN(the_other_way_round_and_between_equal_lines_too)
{
    NEED_FONT();
    ui_lyrics *l = scene(100, 40);
    motion m;

    make_move(l, L_LONG, L_BELOW1, &m); /* tall -> short */
    ASSERT_TRUE(backslide(&m.below1) < 0.01f);
    ASSERT_TRUE(backslide(&m.below3) < 0.01f);
    ASSERT_TRUE(backslide(&m.above) < 0.01f);
    ASSERT_TRUE(backslide(&m.row_below1) < 0.5f);

    make_move(l, L_BELOW1, L_BELOW2, &m); /* short -> short */
    ASSERT_TRUE(backslide(&m.below3) < 0.01f);
    ASSERT_TRUE(backslide(&m.above) < 0.01f);

    make_move(l, L_BELOW3, L_LAST_SHORT, &m);
    ASSERT_TRUE(backslide(&m.above) < 0.01f);
}
TEST_END()

TEST_BEGIN(a_seek_over_several_lines_does_not_double_back)
{
    NEED_FONT();
    ui_lyrics *l = scene(100, 40);
    motion m;
    make_move(l, L_SHORT, L_BELOW3, &m); /* the long line is skipped over */
    ASSERT_TRUE(backslide(&m.above) < 0.01f);
    ASSERT_TRUE(backslide(&m.below1) < 0.01f);
}
TEST_END()

TEST_BEGIN(without_big_text_the_page_still_glides_as_before)
{
    NEED_FONT();
    ui_lyrics *l = scene(100, 40);
    g_u.opt.lyrics.big_rows = 0;
    motion m;
    make_move(l, L_SHORT, L_LONG, &m);
    ASSERT_TRUE(l->grow.n == 0);
    ASSERT_TRUE(backslide(&m.above) < 0.01f);
    ASSERT_TRUE(backslide(&m.below3) < 0.01f);
    /* the glide takes its time rather than snapping: more than a frame or two */
    int moving = 0;
    for (int i = 1; i < m.above.n; i++)
        moving += fabsf(m.above.y[i] - m.above.y[i - 1]) > 0.001f;
    ASSERT_TRUE(moving >= 4);
}
TEST_END()

TEST_BEGIN(the_current_line_comes_to_rest_where_it_always_did)
{
    NEED_FONT();
    ui_lyrics *l = scene(100, 40);
    g_src.timestamp = (int64_t)L_LONG * 3000 * 1000;
    run(l, 3000);
    /* at rest, the middle of the current line is on the reading height */
    float mid = screen_y(l, L_LONG) + (line_h(l, L_LONG) - 1.0f) * 0.5f;
    ASSERT_TRUE(fabsf(mid - ANCHOR * 40.0f) < 0.6f);
    ASSERT_TRUE(!ui_grow_busy(&l->grow, L_LONG));
}
TEST_END()

TEST_BEGIN(changing_the_size_rebuilds_the_big_lines)
{
    NEED_FONT();
    ui_lyrics *l = scene(120, 60);
    g_src.timestamp = (int64_t)L_SHORT * 3000 * 1000;
    run(l, 1500);
    const big *b = big_for(l, L_SHORT);
    ASSERT_TRUE(b != NULL);
    ASSERT_INT_EQ(b->rows, 4);

    g_u.opt.lyrics.big_rows = 6;
    run(l, 100);
    b = big_for(l, L_SHORT);
    ASSERT_TRUE(b != NULL);
    ASSERT_INT_EQ(b->rows, 6); /* not the layout cached for 4 */

    g_u.opt.lyrics.big_rows = 12; /* the old cap was 8 */
    run(l, 100);
    b = big_for(l, L_SHORT);
    ASSERT_TRUE(b != NULL);
    ASSERT_INT_EQ(b->rows, 12);

    g_u.opt.lyrics.big_rows = 0; /* off: the line settles back to small */
    run(l, 1500);
    ASSERT_TRUE(l->grow.n == 0);
    ASSERT_TRUE(big_for(l, L_SHORT) == NULL);
}
TEST_END()

TEST_BEGIN(the_line_being_sung_fills_in_as_its_units_are_sung)
{
    ui_lyrics *l = scene(60, 24);
    g_u.opt.lyrics.big_rows = 0; /* terminal text only: the split is in cells */
    g_src.get_karaoke = fake_get_karaoke;
    give_timing(L_SHORT);

    char t[8][32];
    int xs[8];

    /* before the first unit: all of it is still to come, painted as one piece */
    at(l, 3050);
    ASSERT_INT_EQ(l->current, L_SHORT);
    ASSERT_INT_EQ(g_kfocus, L_SHORT); /* the job is told what is being sung */
    ASSERT_TRUE(l->kf.valid && l->kf.state == KF_BEFORE);
    int n = pieces(t, xs, 0, 23);
    ASSERT_INT_EQ(n, 1);
    ASSERT_TRUE(strcmp(t[0], "Short") == 0);

    /* half way through "ort" (3750): "Sh" and one of its three characters are
     * sung, so the split is after "Shor" and "t" is still dim */
    at(l, 3750);
    ASSERT_TRUE(l->kf.state == KF_INSIDE && l->kf.idx == 1);
    n = pieces(t, xs, 0, 23);
    ASSERT_INT_EQ(n, 2);
    ASSERT_TRUE(strcmp(t[0], "Shor") == 0);
    ASSERT_TRUE(strcmp(t[1], "t") == 0);
    ASSERT_INT_EQ(xs[1], xs[0] + 4); /* the rest starts where the sung part ends */

    /* after the last unit: all sung, one piece again */
    at(l, 4500);
    ASSERT_TRUE(l->kf.state == KF_AFTER);
    n = pieces(t, xs, 0, 23);
    ASSERT_INT_EQ(n, 1);
    ASSERT_TRUE(strcmp(t[0], "Short") == 0);
}
TEST_END()

TEST_BEGIN(a_line_without_timing_yet_is_painted_whole)
{
    ui_lyrics *l = scene(60, 24);
    g_u.opt.lyrics.big_rows = 0;
    g_src.get_karaoke = fake_get_karaoke;
    give_timing(L_LONG); /* the timing is of another line */
    at(l, 3750);
    ASSERT_INT_EQ(l->current, L_SHORT);
    ASSERT_FALSE(l->kf.valid);
    char t[8][32];
    int xs[8];
    ASSERT_INT_EQ(pieces(t, xs, 0, 23), 1);

    /* and when it arrives, the next frame uses it */
    g_kline_for = L_SHORT;
    frame(l, 0.016);
    ASSERT_TRUE(l->kf.valid);
}
TEST_END()

TEST_BEGIN(with_word_timing_off_nothing_is_asked_for)
{
    ui_lyrics *l = scene(60, 24);
    g_u.opt.lyrics.big_rows = 0;
    g_u.opt.lyrics.karaoke = false;
    g_src.get_karaoke = fake_get_karaoke;
    give_timing(L_SHORT);
    at(l, 3750);
    ASSERT_INT_EQ(l->current, L_SHORT);
    ASSERT_INT_EQ(g_kcalls, 0); /* no analysis is started */
    ASSERT_INT_EQ(g_kfocus, -1);
    ASSERT_FALSE(l->kf.valid);
    char t[8][32];
    int xs[8];
    ASSERT_INT_EQ(pieces(t, xs, 0, 23), 1);
}
TEST_END()

TEST_BEGIN(the_offset_moves_the_fill)
{
    ui_lyrics *l = scene(60, 24);
    g_u.opt.lyrics.big_rows = 0;
    g_src.get_karaoke = fake_get_karaoke;
    give_timing(L_SHORT);

    at(l, 3750);
    ASSERT_TRUE(l->kf.state == KF_INSIDE && l->kf.idx == 1);
    /* 400 ms later it is where the music was 400 ms ago: in the first unit */
    g_u.opt.lyrics.karaoke_offset_ms = 400;
    frame(l, 0.016);
    ASSERT_TRUE(l->kf.state == KF_INSIDE && l->kf.idx == 0);
    /* a negative one runs ahead */
    g_u.opt.lyrics.karaoke_offset_ms = -300;
    frame(l, 0.016);
    ASSERT_TRUE(l->kf.state == KF_AFTER);
}
TEST_END()

TEST_BEGIN(it_repaints_while_the_fill_moves_and_not_while_paused)
{
    ui_lyrics *l = scene(60, 24);
    g_u.opt.lyrics.big_rows = 0;
    g_src.get_karaoke = fake_get_karaoke;
    give_timing(L_SHORT);
    at(l, 3200);

    /* playing: the music moves 16 ms a frame through "Sh" */
    int before = g_invalidations;
    for (int i = 0; i < 20; i++)
    {
        g_src.timestamp += 16 * 1000;
        frame(l, 0.016);
    }
    ASSERT_INT_GT(g_invalidations - before, 3);

    /* paused: the position stands still, so the picture does too */
    run(l, 100);
    before = g_invalidations;
    run(l, 400);
    ASSERT_INT_EQ(g_invalidations - before, 0);
}
TEST_END()

TEST_BEGIN(a_kanji_shared_by_two_units_fills_half_at_a_time)
{
    /* "AB" is one glyph pair here; give the second glyph two units */
    ui_lyrics *l = scene(60, 24);
    g_u.opt.lyrics.big_rows = 0;
    g_src.get_karaoke = fake_get_karaoke;
    g_ksyl[0] = (kara_syl){.start_ms = 3100, .end_ms = 3400, .off = 0, .len = 2,
                           .nsub = 1, .conf = 1.0f};
    g_ksyl[1] = (kara_syl){.start_ms = 3400, .end_ms = 3700, .off = 2, .len = 2,
                           .sub = 0, .nsub = 2, .conf = 1.0f};
    g_ksyl[2] = (kara_syl){.start_ms = 3700, .end_ms = 4000, .off = 2, .len = 2,
                           .sub = 1, .nsub = 2, .conf = 1.0f};
    g_kline = (kara_line){.n = 3, .syl = g_ksyl, .conf = 1.0f};
    g_kline_for = L_SHORT;
    at(l, 3850); /* in the third unit, half way: the second half of "or" */
    ASSERT_TRUE(l->kf.state == KF_INSIDE && l->kf.idx == 2);
    ASSERT_INT_EQ(l->kf.nsub, 2);
    ASSERT_INT_EQ(l->kf.sub, 1);
    /* (sub + f) / nsub = (1 + 0.5) / 2 = 0.75 of two characters: 1.5 -> 2 */
    char t[8][32];
    int xs[8];
    int n = pieces(t, xs, 0, 23);
    ASSERT_INT_EQ(n, 2);
    ASSERT_TRUE(strcmp(t[0], "Shor") == 0);
}
TEST_END()

TEST_BEGIN(the_big_line_is_filled_left_to_right_in_pixels)
{
    NEED_FONT();
    ui_lyrics *l = scene(80, 30);
    g_u.theme.primary = (color_t){255, 255, 255, 255};
    g_u.theme.text = (color_t){255, 255, 255, 255};
    g_u.theme.list.normal_bg = (color_t){0, 0, 0, 255};
    g_u.theme.control_bg = (color_t){0, 0, 0, 255};
    g_src.get_karaoke = fake_get_karaoke;
    give_timing(L_SHORT);

    /* the line is big and at rest, nothing sung yet: all of it dim */
    g_src.timestamp = 3050 * 1000LL;
    run(l, 1500);
    ASSERT_INT_EQ(l->current, L_SHORT);
    ASSERT_TRUE(l->kf.valid && l->kf.state == KF_BEFORE);
    int lx, lr, rx, rr;
    ASSERT_TRUE(fb_ends(l, &lx, &lr, &rx, &rr));
    ASSERT_INT_LT(lr, 200);
    ASSERT_INT_LT(rr, 200);
    ASSERT_INT_GT(lr, 60); /* dim, not gone */
    int dim = lr;

    /* part way through the second unit: the left is full ink, the right dim,
     * and the text itself has not moved */
    g_src.timestamp = 3750 * 1000LL;
    run(l, 100);
    int lx2, lr2, rx2, rr2;
    ASSERT_TRUE(fb_ends(l, &lx2, &lr2, &rx2, &rr2));
    ASSERT_INT_EQ(lx2, lx);
    ASSERT_INT_EQ(rx2, rx);
    ASSERT_INT_EQ(lr2, 255);
    ASSERT_INT_EQ(rr2, dim);

    /* all sung: all of it full ink */
    g_src.timestamp = 4500 * 1000LL;
    run(l, 100);
    int lx3, lr3, rx3, rr3;
    ASSERT_TRUE(fb_ends(l, &lx3, &lr3, &rx3, &rr3));
    ASSERT_INT_EQ(lr3, 255);
    ASSERT_INT_EQ(rr3, 255);
}
TEST_END()
