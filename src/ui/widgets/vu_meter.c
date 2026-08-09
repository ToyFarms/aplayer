#include "_math.h"
#include "clock.h"
#include "widgets.h"
#include <math.h>
#include <string.h>

static float lerp(float v0, float v1, float t)
{
    return (1 - t) * v0 + t * v1;
}

static const int _tick_width = 4;
static int tick_width = 4;

static const wchar_t *vu_tick_patterns[] = {L"─", L"╴"};

int vu_meter_get_width(ui_state *state, int height, int nb_channels)
{
    tick_width = state->opt.vu_meter.render_ticks ? _tick_width : 0;
    int tick_gap =
        state->opt.vu_meter.render_ticks ? state->opt.vu_meter.tick_gap : 0;
    return (nb_channels * state->opt.vu_meter.bar_width) +
           ((nb_channels - 1) * state->opt.vu_meter.bar_gap) + tick_width +
           tick_gap + state->opt.vu_meter.right_pad +
           state->opt.vu_meter.left_pad;
}

static color_t vu_meter_level_color(ui_state *state, int y, bool bg)
{
    const char *level;
    if (y >= state->vu_meter_st.mark_red)
        level = "HIGH";
    else if (y >= state->vu_meter_st.mark_yellow)
        level = "MID";
    else
        level = "LOW";

    char key[32];
    snprintf(key, sizeof(key), "VU_METER_%s_%s", level, bg ? "BG" : "FG");
    return GET_THEMECOLOR(state, key);
}

#define VU_ENSURE_CAPACITY(state, arr)                                         \
    do                                                                         \
    {                                                                          \
        if ((arr).capacity < (state)->vu_meter_st.bars.capacity)               \
        {                                                                      \
            array_resize(&(arr), (state)->vu_meter_st.bars.capacity);          \
            (arr).length = (state)->vu_meter_st.bars.capacity;                 \
        }                                                                      \
    } while (0)

static void vu_meter_ensure_buffers(ui_state *state)
{
    VU_ENSURE_CAPACITY(state, state->vu_meter_st.prev_bars);
    VU_ENSURE_CAPACITY(state, state->vu_meter_st.easing_bars);
    VU_ENSURE_CAPACITY(state, state->vu_meter_st.peaks);
    VU_ENSURE_CAPACITY(state, state->vu_meter_st.peak_set);
}

static void vu_meter_reset_anchors(ui_state *state, int max_rows)
{
    if (state->vu_meter_st.anchor_rows.capacity < (size_t)max_rows)
    {
        array_resize(&state->vu_meter_st.anchor_rows, max_rows);
        array_resize(&state->vu_meter_st.anchor_dbfs, max_rows);
    }
    state->vu_meter_st.anchor_rows.length = max_rows;
    state->vu_meter_st.anchor_dbfs.length = max_rows;
    state->vu_meter_st.anchor_count = 0;
}

static void vu_meter_add_anchor(ui_state *state, int row, float dbfs)
{
    int i = state->vu_meter_st.anchor_count;
    if ((size_t)i >= state->vu_meter_st.anchor_rows.length)
        return;

    ARR_AS(state->vu_meter_st.anchor_rows, int)[i] = row;
    ARR_AS(state->vu_meter_st.anchor_dbfs, float)[i] = dbfs;
    state->vu_meter_st.anchor_count = i + 1;
}

static float vu_meter_height_for_dbfs(ui_state *state, float dbfs)
{
    int n = state->vu_meter_st.anchor_count;
    if (n == 0)
        return 0.0f;

    int *rows = ARR_AS(state->vu_meter_st.anchor_rows, int);
    float *vals = ARR_AS(state->vu_meter_st.anchor_dbfs, float);

    if (dbfs <= vals[0])
        return (float)rows[0];
    if (dbfs >= vals[n - 1])
        return (float)rows[n - 1];

    for (int i = 0; i < n - 1; i++)
    {
        if (dbfs >= vals[i] && dbfs <= vals[i + 1])
        {
            float t = (vals[i + 1] - vals[i] > 0.0001f)
                          ? (dbfs - vals[i]) / (vals[i + 1] - vals[i])
                          : 0.0f;
            return lerp((float)rows[i], (float)rows[i + 1], t);
        }
    }
    return (float)rows[n - 1];
}

static int vu_meter_draw_lower_ticks(ui_state *state, str_t *buf, vec2 pos,
                                     int x, int lower)
{
    int ticks = 0;
    for (int i = 0; i < lower; i++)
    {
        if (state->opt.vu_meter.render_ticks)
            term_draw_pos(buf, VEC(x, pos.y - i));

        int nb_ticks = lower / 2;
        if (lower % 2 == 0)
            nb_ticks--;
        float offset = 50.0f / MATH_MAX(nb_ticks, 1);

        if (i % 2 == 0)
        {
            float value = -90.0f + offset * ticks++;
            vu_meter_add_anchor(state, i, value);

            if (state->opt.vu_meter.render_ticks)
            {
                str_catwcs(buf, vu_tick_patterns[0]);
                str_catf(buf, "%.0f", value);
            }
        }
        else if (state->opt.vu_meter.render_ticks)
        {
            str_catwcs(buf, vu_tick_patterns[1]);
        }
    }

    int remainder = lower % 2;
    if (state->opt.vu_meter.render_ticks && remainder != 0)
    {
        term_draw_pos(buf, VEC(x, pos.y - lower));
        str_catwcs(buf, vu_tick_patterns[1]);
    }
    return remainder;
}

static void vu_meter_draw_upper_ticks(ui_state *state, str_t *buf, vec2 pos,
                                      int x, int lower, int remainder,
                                      int upper)
{
    int ticks = 0;
    int last_ticks = 0;
    for (int i = 0; i < upper; i++)
    {
        if (state->opt.vu_meter.render_ticks)
            term_draw_pos(buf, VEC(x, pos.y - (i + lower + remainder)));

        int nb_ticks = upper / 3.0f;
        if (upper % 3 == 0)
            nb_ticks--;
        float offset = 36.0f / nb_ticks;

        float offset_highres = 36.0f / upper;
        float mark_highres = -36.0f + offset_highres * i;

        if (state->vu_meter_st.mark_yellow == 0 && mark_highres > -18.0f)
            state->vu_meter_st.mark_yellow = state->vu_meter_st.high_start + i;
        else if (state->vu_meter_st.mark_red == 0 && mark_highres > -6.0f)
            state->vu_meter_st.mark_red = state->vu_meter_st.high_start + i;

        if (i % 3 == 0)
        {
            float mark = -36.0f + offset * ticks++;
            vu_meter_add_anchor(state, state->vu_meter_st.high_start + i, mark);

            if (state->opt.vu_meter.render_ticks)
            {
                str_catwcs(buf, vu_tick_patterns[0]);
                str_catf(buf, "%3.0f", mark);
            }
            last_ticks = i;
        }
        else if (state->opt.vu_meter.render_ticks)
        {
            str_catwcs(buf, vu_tick_patterns[1]);
        }

        state->vu_meter_st.high_end = upper - last_ticks - 1;
    }
}

static void vu_meter_draw_marks(ui_state *state, str_t *buf, vec2 pos,
                                vec2 size)
{
    if (!state->opt.vu_meter.render_marks)
        return;

    for (int ch = 0; ch < state->vu_meter_st.bars.length; ch++)
    {
        int x =
            pos.x + state->opt.vu_meter.left_pad +
            ch * (state->opt.vu_meter.bar_width + state->opt.vu_meter.bar_gap);
        for (int i = 0; i < size.y; i++)
        {
            term_draw_pos(buf, VEC(x, pos.y - i));
            term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                            (i >= state->vu_meter_st.mark_red ? COLOR(100, 0, 0)
                             : i >= state->vu_meter_st.mark_yellow
                                 ? COLOR(100, 100, 0)
                                 : COLOR(0, 100, 0)));
            str_repeat_wchar(buf, L'—', state->opt.vu_meter.bar_width, NULL);
            term_draw_reset(buf);
        }
    }
}

static void vu_meter_draw_static(ui_state *state, str_t *buf, vec2 pos,
                                 vec2 size, int *lower_io, int *upper_io)
{
    state->vu_meter_st.mark_red = 0;
    state->vu_meter_st.mark_yellow = 0;
    memset(state->vu_meter_st.prev_bars.data, 0,
           state->vu_meter_st.bars.length * sizeof(float));

    vu_meter_reset_anchors(state, size.y);

    term_draw_pos(buf, VEC(pos.x, pos.y - size.y + 1));
    term_draw_rect(buf, VEC(size.x, size.y + 1),
                   GET_THEMECOLOR(state, "VU_METER_BG"), COLOR_NONE);

    term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                    GET_THEMECOLOR(state, "VU_METER_FG"));

    int x = pos.x + size.x - (tick_width + state->opt.vu_meter.right_pad);

    int lower = *lower_io;
    int upper = *upper_io;

    int remainder = vu_meter_draw_lower_ticks(state, buf, pos, x, lower);
    state->vu_meter_st.high_start = lower + remainder;
    upper -= remainder;

    vu_meter_draw_upper_ticks(state, buf, pos, x, lower, remainder, upper);

    vu_meter_add_anchor(state, size.y - 1, 0.0f);

    vu_meter_draw_marks(state, buf, pos, size);

    if (state->opt.vu_meter.render_ticks && state->opt.vu_meter.render_numeric)
    {
        term_draw_pos(buf, VEC(x, pos.y + 1));
        term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        GET_THEMECOLOR(state, "VU_METER_FG"));
        str_cat(buf, "DBFS");
        term_draw_reset(buf);
    }

    if (state->vu_meter_st.bars.length != 0)
        state->vu_meter_st.initialized = true;

    *upper_io = upper;
}

static float vu_meter_update_dbfs(ui_state *state, int ch, float rms_raw)
{
    float dbfs_raw = -0.691f + 10.0f * log10f(rms_raw);
    float dbfs =
        lerp(ARR_AS(state->vu_meter_st.easing_bars, float)[ch], dbfs_raw, 0.5);
    if (isinf(dbfs) || isnan(dbfs) || dbfs < -100)
        dbfs = -99.0f;
    ARR_AS(state->vu_meter_st.easing_bars, float)[ch] = dbfs;

    return dbfs;
}

static float vu_meter_dbfs_to_height(ui_state *state, float dbfs, vec2 size)
{
    float height = vu_meter_height_for_dbfs(state, dbfs);

    return MATH_CLAMP(height, 0, size.y - 1);
}

static void vu_meter_update_peak(ui_state *state, int ch, float height)
{
    if (!state->opt.vu_meter.render_peak)
        return;

    if (height > ARR_AS(state->vu_meter_st.peaks, float)[ch])
    {
        ARR_AS(state->vu_meter_st.peaks, float)[ch] = height;
        ARR_AS(state->vu_meter_st.peak_set, uint64_t)[ch] = gclock_now_ns();
    }
    else if (gclock_now_ns() -
                 ARR_AS(state->vu_meter_st.peak_set, uint64_t)[ch] >
             MS2NS(500))
    {
        ARR_AS(state->vu_meter_st.peaks, float)
        [ch] *= state->opt.vu_meter.peak_decay;
    }
}

static void vu_meter_draw_growth(ui_state *state, str_t *buf, vec2 pos, int x,
                                 int old_height, int new_height)
{
    for (int y = old_height; y <= new_height; y++)
    {
        term_draw_pos(buf, VEC(x, pos.y - y));
        switch (state->opt.vu_meter.style)
        {
        case VU_METER_ANALOG_LINE:
            term_draw_color(buf, COLOR_NONE,
                            vu_meter_level_color(state, y, false));
            str_repeat_wchar(buf, L'—', state->opt.vu_meter.bar_width, NULL);
            break;
        case VU_METER_ANALOG_HALF:
            term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                            vu_meter_level_color(state, y, false));
            str_repeat_wchar(buf, L'▄', state->opt.vu_meter.bar_width, NULL);
            break;
        case VU_METER_ANALOG_BAR:
            term_draw_color(buf, vu_meter_level_color(state, y, false),
                            GET_THEMECOLOR(state, "VU_METER_SEP"));
            if (state->opt.vu_meter.render_separator)
                str_repeat_wchar(buf, L'—', state->opt.vu_meter.bar_width,
                                 NULL);
            else
                term_draw_hline(buf, state->opt.vu_meter.bar_width);
            break;
        default:
            break;
        }
        term_draw_reset(buf);
    }
}

static void vu_meter_draw_shrink(ui_state *state, str_t *buf, vec2 pos,
                                 vec2 size, int x, int old_height,
                                 int new_height)
{
    for (int y = MATH_MIN(old_height + 1, size.y - 1); y > new_height; y--)
    {
        term_draw_pos(buf, VEC(x, pos.y - y));
        term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        vu_meter_level_color(state, y, true));
        if (state->opt.vu_meter.render_marks)
            str_repeat_wchar(buf, L'—', state->opt.vu_meter.bar_width, NULL);
        else
            term_draw_padding(buf, state->opt.vu_meter.bar_width);
        term_draw_reset(buf);
    }
}

static void vu_meter_draw_subcell(ui_state *state, str_t *buf, vec2 pos, int x,
                                  int new_height, float frac)
{
    if (state->opt.vu_meter.style == VU_METER_ANALOG_LINE ||
        state->opt.vu_meter.discrete || frac < 1.0f / 8.0f)
        return;

    int y = new_height + 1;
    term_draw_pos(buf, VEC(x, pos.y - y));
    term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                    vu_meter_level_color(state, y, false));
    for (int w = 0; w < state->opt.vu_meter.bar_width; ++w)
        term_draw_vblockf(buf, frac);
    term_draw_reset(buf);
}

static void vu_meter_draw_peak(ui_state *state, str_t *buf, vec2 pos, vec2 size,
                               int x, int ch, float height)
{
    if (!state->opt.vu_meter.render_peak)
        return;

    int peak_y = ceilf(ARR_AS(state->vu_meter_st.peaks, float)[ch]);
    if (peak_y + 1 <= size.y && !MATH_AROUND(peak_y, height, 1.0))
    {
        term_draw_pos(buf, VEC(x, pos.y - peak_y));
        term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        vu_meter_level_color(state, peak_y, false));
        str_repeat_wchar(buf, L'—', state->opt.vu_meter.bar_width, NULL);

        int y = peak_y + 1;
        term_draw_pos(buf, VEC(x, pos.y - y));
        term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        vu_meter_level_color(state, y, true));
    }
    else
    {
        int y = height + 2;
        term_draw_pos(buf, VEC(x, pos.y - y));
        term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        vu_meter_level_color(state, y, true));
    }

    if (state->opt.vu_meter.render_marks)
        str_repeat_wchar(buf, L'—', state->opt.vu_meter.bar_width, NULL);
    else
        term_draw_hline(buf, state->opt.vu_meter.bar_width);
}

static void vu_meter_draw_numeric(ui_state *state, str_t *buf, vec2 pos, int x,
                                  float dbfs)
{
    if (!state->opt.vu_meter.render_numeric)
        return;

    term_draw_pos(buf, VEC(x, pos.y + 1));
    term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                    GET_THEMECOLOR(state, "VU_METER_FG"));
    if (dbfs == -99.0)
        str_cat(buf, "---");
    else
        str_catf(buf, "%-4.0f", dbfs);
    term_draw_reset(buf);
}

static void vu_meter_draw_channel(ui_state *state, str_t *buf, vec2 pos,
                                  vec2 size, int ch, float rms_raw)
{
    if (isnan(rms_raw))
        return;

    float dbfs = vu_meter_update_dbfs(state, ch, rms_raw);
    float height = vu_meter_dbfs_to_height(state, dbfs, size);

    vu_meter_update_peak(state, ch, height);

    float old_h = ARR_AS(state->vu_meter_st.prev_bars, float)[ch];
    int old_height = (int)old_h;
    int new_height = (int)height;
    float frac = height - new_height;
    int x = pos.x + state->opt.vu_meter.left_pad +
            ch * (state->opt.vu_meter.bar_width + state->opt.vu_meter.bar_gap);

    if (new_height > old_height)
        vu_meter_draw_growth(state, buf, pos, x, old_height, new_height);
    else if (new_height < old_height)
        vu_meter_draw_shrink(state, buf, pos, size, x, old_height, new_height);

    vu_meter_draw_subcell(state, buf, pos, x, new_height, frac);
    vu_meter_draw_peak(state, buf, pos, size, x, ch, height);
    vu_meter_draw_numeric(state, buf, pos, x, dbfs);

    ARR_AS(state->vu_meter_st.prev_bars, float)[ch] = height;
}

void render_vu_meter(ui_state *state, vec2 pos, vec2 size)
{
    if (state->opt.vu_meter.render_numeric)
    {
        size.y -= 1;
        pos.y -= 1;
    }

    vu_meter_ensure_buffers(state);

    str_t *buf = &state->term->buf;
    int lower = size.y * 0.2;
    int upper = size.y - lower;

    if (state->term->resized || !state->vu_meter_st.initialized)
        vu_meter_draw_static(state, buf, pos, size, &lower, &upper);

    if (!state->app->audio->mixer.paused)
    {
        float rms_raw;
        ARR_FOREACH(state->vu_meter_st.bars, rms_raw, ch)
        {
            vu_meter_draw_channel(state, buf, pos, size, ch, rms_raw);
        }
    }

    term_draw_reset(buf);
}
