#include "ui_internal.h"

#include "audio_mixer.h"
#include "audio_scope.h"
#include "image_renderer.h"

#include <math.h>
#include <string.h>

/*
 * The spectrum of what is playing as a single line, the way a DAW's analyzer
 * draws it: low frequencies on the left, high ones on the right, louder is
 * higher. Dots are braille (2 x 4 per cell), so a few rows give a line that
 * is smooth enough to read as a curve.
 *
 * The analyzer keeps APP_SPECTRUM_BANDS levels in app->spectrum (log-spaced,
 * already on a dB scale; see audio_spectrum.h). This widget only
 *
 *  - maps them onto its dot columns (the loudest band wins where several share
 *    a column, neighbours are blended where there are too few),
 *  - rounds the corners a little, and eases every column towards its target:
 *    quick to climb, slower to fall, so the picture breathes instead of
 *    flickering,
 *  - draws the line, and in the filled mode the solid area under it.
 *
 * Paused or nothing loaded counts as silence (the analyzer stops reporting and
 * its last picture must not stick), so the line settles on the floor. It only
 * repaints when a dot actually moved or the colours changed, so a still
 * picture costs nothing.
 *
 * A second mode (`u->spectrum_mode`, switched by an action) turns the same
 * widget into an oscilloscope. Instead of the analyzer's bands it reads the
 * raw samples the audio callback keeps in app->scope (audio_scope.h):
 *
 *  - a window of the most recent samples (scope.window_ms) is laid across the
 *    dot columns, the newest at the right edge; a column that covers several
 *    samples shows their extremes, so a loud passage stays as tall as it is,
 *  - a sample's amplitude is its dot row: silence on the centre row, the
 *    recent peak near the top or bottom one (scope.auto_gain scales for it;
 *    off, a full-scale sample is what reaches the edge),
 *  - the right edge of the window follows the audio at real-time speed, a
 *    little behind the newest sample (scope.delay_ms), and is nudged back
 *    whenever it drifts, so the picture scrolls left smoothly instead of
 *    jumping once per audio block,
 *  - paused or nothing loaded fades the trace to a flat centre line.
 */

#define MAX_COLS  1024 /* dot columns the state can hold */
#define MIN_GLOW  0.55f /* share of its colour a quiet part of the line keeps */
#define FILL_GLOW 0.26f /* colour of the area under the line, at most */

typedef struct spectrum
{
    ui_ctx *u;
    int cols;             /* dot columns in use */
    float v[MAX_COLS];    /* eased level per column, 0..1 */
    int16_t y[MAX_COLS];  /* dot row each column was last painted at */
    int sig_w, sig_h;
    uint32_t sig_rev;

    /* oscilloscope mode */
    int scols;             /* dot columns in use */
    float lo[MAX_COLS];    /* lowest / highest sample of each column, after */
    float hi[MAX_COLS];    /* gain, -1..1 (positive is up) */
    int16_t y0[MAX_COLS];  /* dot rows (top, bottom) each column was last */
    int16_t y1[MAX_COLS];  /* painted at */
    double pos;            /* sample number at the right edge of the window */
    bool pos_valid;
    float level;           /* 1 while playing, eases to 0 in silence */
    float peak;            /* loudest recent sample, for the auto gain */
    enum ui_spectrum_mode sig_mode;
} spectrum;

static void spec_free(tui_node *n, void *ud)
{
    (void)n;
    free(ud);
}

/* bit of the dot at (column 0/1, row 0..3 from the top) in the renderers'
 * 2x4 mask: row * 2 + column */
static uint32_t dot_bit(int col, int row)
{
    return 1u << (row * 2 + col);
}

/* dot row of a level: 0 is the top, rows - 1 the floor */
static float dot_y(float v, int rows)
{
    return (1.0f - MATH_CLAMP(v, 0.0f, 1.0f)) * (float)(rows - 1);
}

/* The analyzer's bands, laid onto `cols` dot columns. */
static void resample(const float *bands, int nb, float *out, int cols)
{
    float per = (float)nb / (float)cols; /* bands per column */
    for (int i = 0; i < cols; i++)
    {
        float x0 = (float)i * per, x1 = x0 + per;
        if (per > 1.0f)
        {
            /* several bands share the column: keep the loudest, so a peak
             * stays as tall at any width */
            int a = (int)x0, b = MATH_MIN((int)ceilf(x1), nb);
            float m = 0.0f;
            for (int j = a; j < b; j++)
                if (bands[j] > m)
                    m = bands[j];
            out[i] = m;
        }
        else
        {
            /* fewer bands than columns: slide between neighbours */
            float p = MATH_CLAMP((x0 + x1) * 0.5f - 0.5f, 0.0f,
                                 (float)(nb - 1));
            int j = (int)p;
            float t = p - (float)j;
            float next = bands[MATH_MIN(j + 1, nb - 1)];
            out[i] = bands[j] + (next - bands[j]) * t;
        }
    }
}

static void line_update(tui_node *n, double dt, void *ud)
{
    spectrum *s = ud;
    ui_ctx *u = s->u;
    const ui_options *o = &u->opt;
    app_instance *app = u->app;

    tui_rect c = tui_get_content_rect(n);
    int cols = MATH_MIN(c.w * 2, MAX_COLS);
    if (cols <= 0 || c.h <= 0)
        return;
    int rows = c.h * 4;

    float want[MAX_COLS];
    bool silent = app->audio->mixer.paused || app_current_source(app) == NULL;
    if (silent)
        memset(want, 0, (size_t)cols * sizeof(float));
    else
    {
        float raw[MAX_COLS];
        resample(app->spectrum.band, APP_SPECTRUM_BANDS, raw, cols);
        /* a light 1-2-1 blur takes the stair steps out of the curve */
        for (int i = 0; i < cols; i++)
        {
            float l = raw[i > 0 ? i - 1 : i], r = raw[i < cols - 1 ? i + 1 : i];
            want[i] = 0.25f * l + 0.5f * raw[i] + 0.25f * r;
        }
    }

    float dts = (float)MATH_MAX(dt, 0.0);
    float up = o->spectrum.attack_ms <= 0.0f
                   ? 1.0f
                   : 1.0f - expf(-dts * 1000.0f / o->spectrum.attack_ms);
    float down = o->spectrum.release_ms <= 0.0f
                     ? 1.0f
                     : 1.0f - expf(-dts * 1000.0f / o->spectrum.release_ms);

    bool changed = c.w != s->sig_w || c.h != s->sig_h ||
                   s->sig_rev != u->ambience.palette_rev || cols != s->cols;
    for (int i = 0; i < cols; i++)
    {
        float k = want[i] > s->v[i] ? up : down;
        s->v[i] += (want[i] - s->v[i]) * k;
        int y = (int)floorf(dot_y(s->v[i], rows) + 0.5f);
        if (y != s->y[i])
        {
            s->y[i] = (int16_t)y;
            changed = true;
        }
    }

    s->cols = cols;
    s->sig_w = c.w;
    s->sig_h = c.h;
    s->sig_rev = u->ambience.palette_rev;
    if (changed)
        tui_invalidate(n);
}

static void line_paint(tui_node *n, tui_canvas *cv, void *ud)
{
    (void)n;
    spectrum *s = ud;
    ui_ctx *u = s->u;
    const ui_palette *pal = &u->theme.palette;
    int W = tui_canvas_width(cv), H = tui_canvas_height(cv);
    int cols = MATH_MIN(W * 2, MAX_COLS);
    if (cols <= 0 || H <= 0 || s->cols != cols)
        return; /* the first update has not run yet */
    int rows = H * 4;

    uint8_t *line = calloc((size_t)W * (size_t)H, 1);
    uint8_t *fill = calloc((size_t)W * (size_t)H, 1);
    if (line == NULL || fill == NULL)
    {
        free(line);
        free(fill);
        return;
    }

    for (int c = 0; c < cols; c++)
    {
        float yc = dot_y(s->v[c], rows);
        float yp = c > 0 ? dot_y(s->v[c - 1], rows) : yc;
        float yn = c < cols - 1 ? dot_y(s->v[c + 1], rows) : yc;
        /* up to the half-way point to each neighbour: where the curve is
         * steep the dots of neighbouring columns meet instead of leaving a
         * gap */
        float a = (yp + yc) * 0.5f, b = (yc + yn) * 0.5f;
        float top = MATH_MIN(yc, MATH_MIN(a, b));
        float bot = MATH_MAX(yc, MATH_MAX(a, b));
        int r0 = (int)floorf(top + 0.5f), r1 = (int)floorf(bot + 0.5f);
        r0 = MATH_CLAMP(r0, 0, rows - 1);
        r1 = MATH_CLAMP(r1, 0, rows - 1);

        for (int r = r0; r <= r1; r++)
            line[(r / 4) * W + c / 2] |= (uint8_t)dot_bit(c & 1, r & 3);
        /* the area under the line, solid down to the floor; in the line-only
         * mode just the line is drawn, and everything below it stays clear */
        if (u->spectrum_mode == UI_SPECTRUM_MODE_LINE_FILL)
            for (int r = r1 + 1; r < rows; r++)
                fill[(r / 4) * W + c / 2] |= (uint8_t)dot_bit(c & 1, r & 3);
    }

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            uint8_t l = line[y * W + x], f = fill[y * W + x];
            if (!l && !f)
                continue;
            /* the hue walks from the first accent (bass) to the second
             * (treble); a louder part of the line burns brighter */
            float pos = ((float)x + 0.5f) / (float)W;
            color_t hue = color_mix(pal->accent, pal->accent2, pos);
            float lv = MATH_MAX(s->v[MATH_MIN(x * 2, cols - 1)],
                                s->v[MATH_MIN(x * 2 + 1, cols - 1)]);
            /* line and filled area share one colour, so the fill reads as
             * a continuation of the line rather than a second layer */
            float glow = MIN_GLOW + (1.0f - MIN_GLOW) * lv;
            color_t ink = color_mix(pal->bg, hue, glow);
            tui_canvas_put(cv, x, y, image_mask_to_braille((uint32_t)(l | f)),
                           ui_pen(ink, COLOR_NONE));
        }
    free(line);
    free(fill);
}

/* ------------------------------------------------------------------------ */
/* oscilloscope                                                             */
/* ------------------------------------------------------------------------ */

#define SCOPE_SNAP   0.15  /* seconds of drift after which the window jumps */
#define SCOPE_NUDGE  0.1   /* share of the drift corrected per update */
#define SCOPE_PEAK_FLOOR   0.05f /* quieter than this is not boosted further */
#define SCOPE_PEAK_FILL    0.9f  /* share of the height the peak is scaled to */
#define SCOPE_PEAK_FALL_S  1.2f  /* seconds for the scale to relax after a peak */
#define SCOPE_GRID   0.5f  /* colour of the centre line, relative to FILL_GLOW */

/* dot row of a sample: +1 is the top row, -1 the bottom one */
static float scope_y(float a, int rows)
{
    return dot_y(0.5f + 0.5f * a, rows);
}

static void scope_update(tui_node *n, double dt, void *ud)
{
    spectrum *s = ud;
    ui_ctx *u = s->u;
    const ui_options *o = &u->opt;
    app_instance *app = u->app;
    const audio_scope *sc = &app->scope;

    tui_rect c = tui_get_content_rect(n);
    int cols = MATH_MIN(c.w * 2, MAX_COLS);
    if (cols <= 0 || c.h <= 0)
        return;
    int rows = c.h * 4;

    uint64_t head = sc->head;
    int sr = sc->sample_rate;
    bool silent = app->audio->mixer.paused || app_current_source(app) == NULL ||
                  sr <= 0;

    float dts = (float)MATH_MAX(dt, 0.0);
    float up = o->spectrum.attack_ms <= 0.0f
                   ? 1.0f
                   : 1.0f - expf(-dts * 1000.0f / o->spectrum.attack_ms);
    float down = o->spectrum.release_ms <= 0.0f
                     ? 1.0f
                     : 1.0f - expf(-dts * 1000.0f / o->spectrum.release_ms);
    float goal = silent ? 0.0f : 1.0f;
    s->level += (goal - s->level) * (goal > s->level ? up : down);

    /* the window: `win` samples, ending at s->pos */
    double rate = sr > 0 ? (double)sr : 48000.0;
    double win = (double)o->scope.window_ms * 0.001 * rate;
    win = MATH_CLAMP(win, (double)cols, (double)(AUDIO_SCOPE_SIZE / 2));
    double lag = (double)o->scope.delay_ms * 0.001 * rate;
    lag = MATH_CLAMP(lag, 0.0, (double)(AUDIO_SCOPE_SIZE / 4));

    if (sr > 0)
    {
        double want = (double)head - lag; /* where the window should end */
        double oldest = (double)head - (double)AUDIO_SCOPE_SIZE + win;
        if (!s->pos_valid || fabs(want - s->pos) > SCOPE_SNAP * rate ||
            s->pos < oldest)
        {
            s->pos = want; /* first sight, a stall, a seek: start over */
            s->pos_valid = true;
        }
        else
        {
            /* the scrolling: real time moves the window on, and the pull
             * towards `want` irons out the audio arriving in blocks */
            if (!silent)
                s->pos += (double)dts * rate;
            s->pos += (want - s->pos) * SCOPE_NUDGE;
        }
        if (s->pos > (double)head)
            s->pos = (double)head;
    }

    bool changed = c.w != s->sig_w || c.h != s->sig_h ||
                   s->sig_rev != u->ambience.palette_rev || cols != s->scols;
    double start = s->pos - win;
    double per = win / (double)cols; /* samples per column */
    /* what is in each column, before any scaling */
    float frame_peak = 0.0f;
    for (int i = 0; i < cols; i++)
    {
        int64_t i0 = (int64_t)floor(start + (double)i * per);
        int64_t i1 = (int64_t)floor(start + (double)(i + 1) * per);
        int count = (int)MATH_MAX(i1 - i0, 1);
        float lo = 0.0f, hi = 0.0f;
        if (sr > 0 && i0 >= 0)
            audio_scope_minmax(sc, (uint64_t)i0, count, &lo, &hi);
        s->lo[i] = lo;
        s->hi[i] = hi;
        frame_peak = MATH_MAX(frame_peak, MATH_MAX(fabsf(lo), fabsf(hi)));
    }

    /* Auto gain: music is mastered (and levelled by the autogain effect) well
     * below full scale, so at 1:1 the trace would hug the centre line. The
     * scale follows the recent peak: it drops at once on a louder sample and
     * relaxes slowly afterwards, so it does not pump with every beat. While
     * silent the peak is left alone, and the fade-out takes care of the trace. */
    if (!silent)
    {
        if (frame_peak > s->peak)
            s->peak = frame_peak;
        else
            s->peak += (frame_peak - s->peak) *
                       (1.0f - expf(-dts / SCOPE_PEAK_FALL_S));
    }
    float norm = 1.0f;
    if (o->scope.auto_gain)
        norm = SCOPE_PEAK_FILL / MATH_MAX(s->peak, SCOPE_PEAK_FLOOR);
    float gain = o->scope.gain * norm * s->level;

    for (int i = 0; i < cols; i++)
    {
        float lo = MATH_CLAMP(s->lo[i] * gain, -1.0f, 1.0f);
        float hi = MATH_CLAMP(s->hi[i] * gain, -1.0f, 1.0f);
        s->lo[i] = lo;
        s->hi[i] = hi;

        int q0 = (int)floorf(scope_y(hi, rows) + 0.5f);
        int q1 = (int)floorf(scope_y(lo, rows) + 0.5f);
        if (q0 != s->y0[i] || q1 != s->y1[i])
        {
            s->y0[i] = (int16_t)q0;
            s->y1[i] = (int16_t)q1;
            changed = true;
        }
    }

    s->scols = cols;
    s->sig_w = c.w;
    s->sig_h = c.h;
    s->sig_rev = u->ambience.palette_rev;
    if (changed)
        tui_invalidate(n);
}

static void scope_paint(tui_node *n, tui_canvas *cv, void *ud)
{
    (void)n;
    spectrum *s = ud;
    ui_ctx *u = s->u;
    const ui_palette *pal = &u->theme.palette;
    int W = tui_canvas_width(cv), H = tui_canvas_height(cv);
    int cols = MATH_MIN(W * 2, MAX_COLS);
    if (cols <= 0 || H <= 0 || s->scols != cols)
        return; /* the first update has not run yet */
    int rows = H * 4;

    uint8_t *line = calloc((size_t)W * (size_t)H, 1);
    uint8_t *grid = calloc((size_t)W * (size_t)H, 1);
    if (line == NULL || grid == NULL)
    {
        free(line);
        free(grid);
        return;
    }

    if (u->opt.scope.graticule)
    {
        int r = (int)floorf(scope_y(0.0f, rows) + 0.5f);
        for (int c = 0; c < cols; c += 4)
            grid[(r / 4) * W + c / 2] |= (uint8_t)dot_bit(c & 1, r & 3);
    }

    for (int c = 0; c < cols; c++)
    {
        float top = scope_y(s->hi[c], rows), bot = scope_y(s->lo[c], rows);
        float mid = (top + bot) * 0.5f;
        float mp = c > 0 ? (scope_y(s->hi[c - 1], rows) +
                            scope_y(s->lo[c - 1], rows)) * 0.5f
                         : mid;
        float mn = c < cols - 1 ? (scope_y(s->hi[c + 1], rows) +
                                   scope_y(s->lo[c + 1], rows)) * 0.5f
                                : mid;
        /* up to the half-way point to each neighbour, so a fast swing joins
         * up with the next column instead of leaving a gap */
        float a = (mp + mid) * 0.5f, b = (mid + mn) * 0.5f;
        float t = MATH_MIN(top, MATH_MIN(a, b));
        float d = MATH_MAX(bot, MATH_MAX(a, b));
        int r0 = MATH_CLAMP((int)floorf(t + 0.5f), 0, rows - 1);
        int r1 = MATH_CLAMP((int)floorf(d + 0.5f), 0, rows - 1);
        for (int r = r0; r <= r1; r++)
            line[(r / 4) * W + c / 2] |= (uint8_t)dot_bit(c & 1, r & 3);
    }

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            uint8_t l = line[y * W + x], g = grid[y * W + x];
            if (!l && !g)
                continue;
            /* the hue walks from the first accent (oldest, left) to the
             * second (newest, right), and the trace is brightest where it
             * is newest and loud */
            float pos = ((float)x + 0.5f) / (float)W;
            color_t hue = color_mix(pal->accent, pal->accent2, pos);
            int c0 = MATH_MIN(x * 2, cols - 1), c1 = MATH_MIN(x * 2 + 1, cols - 1);
            float mag = MATH_MAX(MATH_MAX(fabsf(s->lo[c0]), fabsf(s->hi[c0])),
                                 MATH_MAX(fabsf(s->lo[c1]), fabsf(s->hi[c1])));
            float glow = l ? MATH_CLAMP(0.30f + 0.40f * pos + 0.30f * mag, 0.0f,
                                        1.0f)
                           : FILL_GLOW * SCOPE_GRID;
            color_t ink = color_mix(pal->bg, hue, glow);
            tui_canvas_put(cv, x, y, image_mask_to_braille((uint32_t)(l | g)),
                           ui_pen(ink, COLOR_NONE));
        }
    free(line);
    free(grid);
}

/* ------------------------------------------------------------------------ */
/* the widget: whichever mode is on                                         */
/* ------------------------------------------------------------------------ */

static void spec_update(tui_node *n, double dt, void *ud)
{
    spectrum *s = ud;
    enum ui_spectrum_mode mode = s->u->spectrum_mode;
    bool switched = mode != s->sig_mode;
    s->sig_mode = mode;

    if (mode == UI_SPECTRUM_MODE_SCOPE)
        scope_update(n, dt, ud);
    else
        line_update(n, dt, ud);
    if (switched)
        tui_invalidate(n);
}

static void spec_paint(tui_node *n, tui_canvas *cv, void *ud)
{
    spectrum *s = ud;
    if (s->u->spectrum_mode == UI_SPECTRUM_MODE_SCOPE)
        scope_paint(n, cv, ud);
    else
        line_paint(n, cv, ud);
}

tui_node *ui_spectrum_new(ui_ctx *u, int rows)
{
    spectrum *s = calloc(1, sizeof(*s));
    if (s == NULL)
        abort();
    s->u = u;
    s->sig_w = s->sig_h = -1;
    s->sig_mode = u->spectrum_mode;
    for (int i = 0; i < MAX_COLS; i++)
        s->y[i] = s->y0[i] = s->y1[i] = -1;

    tui_node *n = tui__open(u->tui, TUI_KIND_BOX,
                            &(tui_attr){._init = 0,
                                        .height = TUI_CELLS(rows),
                                        .shrink = TUI_NO_SHRINK,
                                        .paint = spec_paint,
                                        .on_update = spec_update,
                                        .on_destroy = spec_free,
                                        .userdata = s});
    tui__close(u->tui);
    return n;
}
