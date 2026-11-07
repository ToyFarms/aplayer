#include "ui_internal.h"

#include "audio_mixer.h"
#include "clock.h"

#include <math.h>
#include <string.h>

/*
 * VU meter. Levels come from app->meters; this widget only turns them into
 * pictures. State that evolves over time (easing, peak hold) is advanced in
 * on_update; paint only draws what the state says, so it can run any number of
 * times (partial repaints) without changing the picture.
 */

#define TICK_W 4

typedef struct vu
{
    ui_ctx *u;
    int rows; /* bar rows (excludes the ACT and numeric rows) */
    int nch;  /* channels the per-channel arrays are sized for */
    float *easing, *height, *dbfs, *peak;
    uint64_t *peak_set;

    int anchor_count;
    int *anchor_rows;
    float *anchor_dbfs;
    uint8_t *tick_kind; /* per row: 0 none, 1 labelled, 2 minor */
    float *tick_val;
    int mark_red, mark_yellow, high_start;
} vu;

static float lerpf(float a, float b, float t)
{
    return (1 - t) * a + t * b;
}

static void vu_free(tui_node *n, void *ud)
{
    (void)n;
    vu *v = ud;
    free(v->easing);
    free(v->height);
    free(v->dbfs);
    free(v->peak);
    free(v->peak_set);
    free(v->anchor_rows);
    free(v->anchor_dbfs);
    free(v->tick_kind);
    free(v->tick_val);
    free(v);
}

static void *grow(void *p, size_t n, size_t sz)
{
    void *q = realloc(p, n * sz);
    if (q == NULL)
        abort();
    return q;
}

static void add_anchor(vu *v, int row, float dbfs, int cap)
{
    if (v->anchor_count >= cap)
        return;
    v->anchor_rows[v->anchor_count] = row;
    v->anchor_dbfs[v->anchor_count] = dbfs;
    v->anchor_count++;
}

/* tick labels, dBFS anchors and the yellow/red marks for `rows` bar rows */
static void vu_layout(vu *v, int rows)
{
    v->rows = rows;
    v->anchor_rows = grow(v->anchor_rows, (size_t)rows + 2, sizeof(int));
    v->anchor_dbfs = grow(v->anchor_dbfs, (size_t)rows + 2, sizeof(float));
    v->tick_kind = grow(v->tick_kind, (size_t)rows + 2, 1);
    v->tick_val = grow(v->tick_val, (size_t)rows + 2, sizeof(float));
    memset(v->tick_kind, 0, (size_t)rows + 2);
    int cap = rows + 2;

    v->anchor_count = 0;
    v->mark_red = v->mark_yellow = 0;

    int lower = (int)(rows * 0.2);
    int upper = rows - lower;

    int ticks = 0;
    int nb_ticks = lower / 2;
    if (lower % 2 == 0)
        nb_ticks--;
    float offset = 50.0f / (float)MATH_MAX(nb_ticks, 1);
    for (int i = 0; i < lower; i++)
    {
        if (i % 2 == 0)
        {
            float value = -90.0f + offset * (float)ticks++;
            add_anchor(v, i, value, cap);
            v->tick_kind[i] = 1;
            v->tick_val[i] = value;
        }
        else
            v->tick_kind[i] = 2;
    }

    int remainder = lower % 2;
    if (remainder)
        v->tick_kind[lower] = 2;
    v->high_start = lower + remainder;
    upper -= remainder;

    ticks = 0;
    nb_ticks = (int)(upper / 3.0f);
    if (upper % 3 == 0)
        nb_ticks--;
    offset = 36.0f / (float)MATH_MAX(nb_ticks, 1);
    float highres = upper > 0 ? 36.0f / (float)upper : 0.0f;
    for (int i = 0; i < upper; i++)
    {
        float mark_highres = -36.0f + highres * (float)i;
        if (v->mark_yellow == 0 && mark_highres > -18.0f)
            v->mark_yellow = v->high_start + i;
        else if (v->mark_red == 0 && mark_highres > -6.0f)
            v->mark_red = v->high_start + i;

        int row = v->high_start + i;
        if (i % 3 == 0)
        {
            float mark = -36.0f + offset * (float)ticks++;
            add_anchor(v, row, mark, cap);
            v->tick_kind[row] = 1;
            v->tick_val[row] = mark;
        }
        else
            v->tick_kind[row] = 2;
    }
    add_anchor(v, rows - 1, 0.0f, cap);
}

static float height_for_dbfs(const vu *v, float dbfs)
{
    int n = v->anchor_count;
    if (n == 0)
        return 0.0f;
    if (dbfs <= v->anchor_dbfs[0])
        return (float)v->anchor_rows[0];
    if (dbfs >= v->anchor_dbfs[n - 1])
        return (float)v->anchor_rows[n - 1];
    for (int i = 0; i < n - 1; i++)
    {
        float a = v->anchor_dbfs[i], b = v->anchor_dbfs[i + 1];
        if (dbfs >= a && dbfs <= b)
        {
            float t = (b - a > 0.0001f) ? (dbfs - a) / (b - a) : 0.0f;
            return lerpf((float)v->anchor_rows[i], (float)v->anchor_rows[i + 1],
                         t);
        }
    }
    return (float)v->anchor_rows[n - 1];
}

static int vu_width(ui_ctx *u, int nch)
{
    const ui_options *o = &u->opt;
    int tick_w = o->vu_meter.render_ticks ? TICK_W : 0;
    int tick_gap = o->vu_meter.render_ticks ? o->vu_meter.tick_gap : 0;
    return nch * o->vu_meter.bar_width + (nch - 1) * o->vu_meter.bar_gap +
           tick_w + tick_gap + o->vu_meter.right_pad + o->vu_meter.left_pad;
}

static enum ui_level level_at(const vu *v, int y)
{
    return y >= v->mark_red      ? UI_LEVEL_HIGH
           : y >= v->mark_yellow ? UI_LEVEL_MID
                                 : UI_LEVEL_LOW;
}

static color_t level_color(ui_ctx *u, const vu *v, int y, bool bg)
{
    const ui_theme *th = &u->theme;
    enum ui_level level = level_at(v, y);
    return bg ? th->vu_meter.level_bg[level] : th->vu_meter.level_fg[level];
}

/* ------------------------------------------------------------------------ */

static void vu_update(tui_node *n, double dt, void *ud)
{
    (void)dt;
    vu *v = ud;
    ui_ctx *u = v->u;
    const ui_options *o = &u->opt;
    int nch = u->app->meters.rms.channels;

    /* the meter's width follows the channel count */
    int want_w = vu_width(u, nch);
    tui_attr a = tui_get_attr(n);
    bool bg_differs =
        memcmp(&a.bg, &u->theme.vu_meter.bg, sizeof(color_t)) != 0;
    if (a.width.unit != TUI_UNIT_CELLS || (int)a.width.v != want_w ||
        bg_differs)
    {
        a.width = TUI_CELLS(want_w);
        a.bg = u->theme.vu_meter.bg; /* the theme may change under us */
        tui_set_attr(n, &a);
    }

    tui_rect c = tui_get_content_rect(n);
    int rows = c.h - 1 - (o->vu_meter.render_numeric ? 1 : 0);
    if (rows < 2 || c.w <= 0)
        return;

    if (v->rows != rows)
    {
        vu_layout(v, rows);
        if (v->nch > 0)
            memset(v->height, 0, (size_t)v->nch * sizeof(float));
        tui_invalidate(n);
    }

    if (nch > v->nch)
    {
        int old = v->nch;
        v->easing = grow(v->easing, (size_t)nch, sizeof(float));
        v->height = grow(v->height, (size_t)nch, sizeof(float));
        v->dbfs = grow(v->dbfs, (size_t)nch, sizeof(float));
        v->peak = grow(v->peak, (size_t)nch, sizeof(float));
        v->peak_set = grow(v->peak_set, (size_t)nch, sizeof(uint64_t));
        for (int i = old; i < nch; i++)
        {
            v->easing[i] = v->height[i] = v->peak[i] = 0.0f;
            v->dbfs[i] = -99.0f;
            v->peak_set[i] = 0;
        }
        v->nch = nch;
        tui_invalidate(n);
    }

    if (u->app->audio->mixer.paused)
        return;

    bool changed = false;
    uint64_t now = gclock_now_ns();
    for (int ch = 0; ch < nch; ch++)
    {
        float rms = u->app->meters.rms.level[ch];
        if (isnan(rms))
            continue;

        float dbfs_raw = -0.691f + 10.0f * log10f(rms);
        float dbfs = lerpf(v->easing[ch], dbfs_raw, 0.5f);
        if (isinf(dbfs) || isnan(dbfs) || dbfs < -100)
            dbfs = -99.0f;
        v->easing[ch] = dbfs;

        float height = MATH_CLAMP(height_for_dbfs(v, dbfs), 0, rows - 1);

        if (o->vu_meter.render_peak)
        {
            if (height > v->peak[ch])
            {
                v->peak[ch] = height;
                v->peak_set[ch] = now;
            }
            else if (now - v->peak_set[ch] > MS2NS(500))
                v->peak[ch] *= o->vu_meter.peak_decay;
        }

        if ((int)(height * 8) != (int)(v->height[ch] * 8) ||
            (int)dbfs != (int)v->dbfs[ch])
            changed = true;
        v->height[ch] = height;
        v->dbfs[ch] = dbfs;
    }
    /* peaks decay continuously, so keep repainting while any are held */
    if (o->vu_meter.render_peak)
        changed = true;
    if (changed)
        tui_invalidate(n);
}

static void repeat(tui_canvas *cv, int x, int y, int n, uint32_t cp,
                   tui_pen pen)
{
    for (int i = 0; i < n; i++)
        tui_canvas_put(cv, x + i, y, cp, pen);
}

static void vu_paint(tui_node *n, tui_canvas *cv, void *ud)
{
    (void)n;
    vu *v = ud;
    ui_ctx *u = v->u;
    const ui_options *o = &u->opt;
    const ui_theme *th = &u->theme;
    const app_level_meter *act = &u->app->meters.activity;

    int W = tui_canvas_width(cv), H = tui_canvas_height(cv);
    color_t bg = th->vu_meter.bg, fg = th->vu_meter.fg;
    tui_canvas_fill(cv, (tui_rect){0, 0, W, H}, ' ', ui_pen(fg, bg));

    int numeric = o->vu_meter.render_numeric ? 1 : 0;
    int rows = H - 1 - numeric;
    int nch = u->app->meters.rms.channels;
    int bw = o->vu_meter.bar_width;

    /* --- ACT row (energy per band) --- */
    {
        for (int i = 0; i < act->channels; i++)
        {
            float e = act->level[i];
            if (isnan(e) || isinf(e))
                continue;
            color_t c = color_mix(th->vu_meter.act_off, th->vu_meter.act_on,
                                  MATH_CLAMP(e, 0.0f, 1.0f));
            repeat(cv, o->vu_meter.left_pad + i * (o->vu_meter.bar_gap + bw), 0,
                   bw, ' ', ui_pen(COLOR_NONE, c));
        }
        int x =
            o->vu_meter.left_pad + act->channels * (o->vu_meter.bar_gap + bw);
        tui_canvas_text(cv, x, 0, "ACT", ui_pen(th->primary, COLOR_NONE));
    }

    if (rows < 2 || v->rows != rows)
        return; /* first update has not run yet */

    int base = H - 1 - numeric; /* screen row of level 0 */
    int tx = W - (TICK_W + o->vu_meter.right_pad);
    tui_pen tick = ui_pen(fg, bg);

    /* --- ticks --- */
    if (o->vu_meter.render_ticks)
    {
        for (int i = 0; i < rows; i++)
        {
            int y = base - i;
            if (v->tick_kind[i] == 1)
            {
                char t[16];
                snprintf(t, sizeof(t), i < v->high_start ? "%.0f" : "%3.0f",
                         v->tick_val[i]);
                tui_canvas_put(cv, tx, y, 0x2500, tick); /* ─ */
                tui_canvas_text(cv, tx + 1, y, t, tick);
            }
            else if (v->tick_kind[i] == 2)
                tui_canvas_put(cv, tx, y, 0x2574, tick); /* ╴ */
        }
    }

    /* --- bars --- */
    for (int ch = 0; ch < nch && ch < v->nch; ch++)
    {
        int x = o->vu_meter.left_pad + ch * (bw + o->vu_meter.bar_gap);
        float height = v->height[ch];
        int top = (int)height;
        float frac = height - (float)top;

        for (int y = 0; y < rows; y++)
        {
            int sy = base - y;
            if (y <= top && height > 0.0f)
            {
                switch (o->vu_meter.style)
                {
                case UI_VU_STYLE_LINE:
                    repeat(cv, x, sy, bw, 0x2014,
                           ui_pen(level_color(u, v, y, false), bg));
                    break;
                case UI_VU_STYLE_HALF:
                    repeat(cv, x, sy, bw, 0x2584,
                           ui_pen(level_color(u, v, y, false), bg));
                    break;
                case UI_VU_STYLE_BAR:
                    repeat(
                        cv, x, sy, bw,
                        o->vu_meter.render_separator ? 0x2014 : ' ',
                        ui_pen(th->vu_meter.sep, level_color(u, v, y, false)));
                    break;
                }
            }
            else
            {
                repeat(cv, x, sy, bw, o->vu_meter.render_marks ? 0x2014 : ' ',
                       ui_pen(level_color(u, v, y, true), bg));
            }
        }

        /* sub-cell cap above the last full row */
        int cy = top + 1;
        if (o->vu_meter.style != UI_VU_STYLE_LINE && !o->vu_meter.discrete &&
            frac >= 1.0f / 8.0f && cy < rows)
        {
            uint32_t blk = 0x2580 + (uint32_t)(frac * 8.0f); /* ▁..█ */
            if (blk < 0x2581)
                blk = 0x2581;
            if (blk > 0x2588)
                blk = 0x2588;
            repeat(cv, x, base - cy, bw, blk,
                   ui_pen(level_color(u, v, cy, false), bg));
        }

        /* peak hold */
        if (o->vu_meter.render_peak)
        {
            int py = (int)ceilf(v->peak[ch]);
            if (py + 1 <= rows && !MATH_AROUND((float)py, height, 1.0f) &&
                py < rows)
                repeat(cv, x, base - py, bw, 0x2014,
                       ui_pen(level_color(u, v, py, false), bg));
        }

        /* numeric readout */
        if (numeric)
        {
            char t[16];
            if (v->dbfs[ch] == -99.0f)
                snprintf(t, sizeof(t), "---");
            else
                snprintf(t, sizeof(t), "%-4.0f", v->dbfs[ch]);
            tui_canvas_text(cv, x, H - 1, t, ui_pen(fg, bg));
        }
    }

    if (numeric && o->vu_meter.render_ticks)
        tui_canvas_text(cv, tx, H - 1, "DBFS", ui_pen(fg, bg));
}

tui_node *ui_vu_meter_new(ui_ctx *u)
{
    vu *v = calloc(1, sizeof(*v));
    if (v == NULL)
        abort();
    v->u = u;

    tui_node *n = tui__open(u->tui, TUI_KIND_BOX,
                            &(tui_attr){._init = 0,
                                        .width = TUI_CELLS(vu_width(u, 0)),
                                        .shrink = TUI_NO_SHRINK,
                                        .bg = u->theme.vu_meter.bg,
                                        .paint = vu_paint,
                                        .on_update = vu_update,
                                        .on_destroy = vu_free,
                                        .userdata = v});
    tui__close(u->tui);
    return n;
}
