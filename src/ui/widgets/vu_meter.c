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
static int tick_width = _tick_width;

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

void render_vu_meter(ui_state *state, vec2 pos, vec2 size)
{
    if (state->opt.vu_meter.render_numeric)
    {
        size.y -= 1;
        pos.y -= 1;
    }
    static const wchar_t *patterns[] = {L"─", L"╴"};

    if (state->vu_meter_st.prev_bars.capacity <
        state->vu_meter_st.bars.capacity)
    {
        array_resize(&state->vu_meter_st.prev_bars,
                     state->vu_meter_st.bars.capacity);
        state->vu_meter_st.prev_bars.length = state->vu_meter_st.bars.capacity;
    }

    if (state->vu_meter_st.easing_bars.capacity <
        state->vu_meter_st.bars.capacity)
    {
        array_resize(&state->vu_meter_st.easing_bars,
                     state->vu_meter_st.bars.capacity);
        state->vu_meter_st.easing_bars.length =
            state->vu_meter_st.bars.capacity;
    }

    if (state->vu_meter_st.peaks.capacity < state->vu_meter_st.bars.capacity)
    {
        array_resize(&state->vu_meter_st.peaks,
                     state->vu_meter_st.bars.capacity);
        state->vu_meter_st.peaks.length = state->vu_meter_st.bars.capacity;
    }

    if (state->vu_meter_st.peak_set.capacity < state->vu_meter_st.bars.capacity)
    {
        array_resize(&state->vu_meter_st.peak_set,
                     state->vu_meter_st.bars.capacity);
        state->vu_meter_st.peak_set.length = state->vu_meter_st.bars.capacity;
    }

    str_t *buf = &state->term->buf;
    int lower = size.y * 0.2;
    int upper = size.y - lower;

    if (state->term->resized || !state->vu_meter_st.initialized)
    {
        state->vu_meter_st.mark_red = 0;
        state->vu_meter_st.mark_yellow = 0;
        memset(state->vu_meter_st.prev_bars.data, 0,
               state->vu_meter_st.bars.length * sizeof(float));

        term_draw_pos(buf, VEC(pos.x, pos.y - size.y));
        term_draw_rect(buf, VEC(size.x, size.y + 1),
                       GET_THEMECOLOR(state, "VU_METER_BG"), COLOR_NONE);

        term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        GET_THEMECOLOR(state, "VU_METER_FG"));

        int x = pos.x + size.x - (tick_width + state->opt.vu_meter.right_pad);

        int ticks = 0;
        for (int i = 0; i < lower; i++)
        {
            if (state->opt.vu_meter.render_ticks)
                term_draw_pos(buf, VEC(x, pos.y - i));

            int nb_ticks = lower / 2;
            if (lower % 2 == 0)
                nb_ticks--;
            float offset = 50.0f / MATH_MAX(nb_ticks, 1);

            if (state->opt.vu_meter.render_ticks)
            {
                if (i % 2 == 0)
                {
                    str_catwcs(buf, patterns[0]);
                    str_catf(buf, "%.0f", -90.0f + offset * ticks++);
                }
                else
                {
                    str_catwcs(buf, patterns[1]);
                }
            }
        }

        int remainder = lower % 2;
        if (state->opt.vu_meter.render_ticks && remainder != 0)
        {
            term_draw_pos(buf, VEC(x, pos.y - lower));
            str_catwcs(buf, patterns[1]);
        }
        state->vu_meter_st.high_start = lower + remainder;
        upper -= remainder;

        ticks = 0;
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
                state->vu_meter_st.mark_yellow =
                    state->vu_meter_st.high_start + i;
            else if (state->vu_meter_st.mark_red == 0 && mark_highres > -6.0f)
                state->vu_meter_st.mark_red = state->vu_meter_st.high_start + i;

            if (state->opt.vu_meter.render_ticks)
            {
                if (i % 3 == 0)
                {
                    float mark = -36.0f + offset * ticks++;

                    str_catwcs(buf, patterns[0]);
                    str_catf(buf, "%3.0f", mark);
                    last_ticks = i;
                }
                else
                {
                    str_catwcs(buf, patterns[1]);
                }
            }

            state->vu_meter_st.high_end = upper - last_ticks - 1;
        }

        if (state->opt.vu_meter.render_marks)
        {
            for (int ch = 0; ch < state->vu_meter_st.bars.length; ch++)
            {
                int x = pos.x + state->opt.vu_meter.left_pad +
                        ch * (state->opt.vu_meter.bar_width +
                              state->opt.vu_meter.bar_gap);
                for (int i = 0; i < size.y; i++)
                {
                    term_draw_pos(buf, VEC(x, pos.y - i));
                    term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                                    (i >= state->vu_meter_st.mark_red
                                         ? COLOR(100, 0, 0)
                                     : i >= state->vu_meter_st.mark_yellow
                                         ? COLOR(100, 100, 0)
                                         : COLOR(0, 100, 0)));
                    str_repeat_wchar(buf, L'—', state->opt.vu_meter.bar_width,
                                     NULL);
                    term_draw_reset(buf);
                }
            }
        }

        if (state->opt.vu_meter.render_ticks &&
            state->opt.vu_meter.render_numeric)
        {
            term_draw_pos(buf, VEC(x, pos.y + 1));
            term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                            GET_THEMECOLOR(state, "VU_METER_FG"));
            str_cat(buf, "dBFS");
            term_draw_reset(buf);
        }

        // sometimes the audio is not loaded yet (rms callback set bars length)
        // making the width wrong (vu_meter_get_width) because the channel is
        // unknown yet
        if (state->vu_meter_st.bars.length != 0)
            state->vu_meter_st.initialized = true;
    }

    if (!state->app->audio->mixer.paused)
    {
#define PEAK_RMS 9.0f
        float rms_raw;
        ARR_FOREACH(state->vu_meter_st.bars, rms_raw, ch)
        {
            if (isnan(rms_raw))
                continue;

            float dbfs_raw = 20 * log10f(rms_raw / PEAK_RMS);
            float dbfs = lerp(ARR_AS(state->vu_meter_st.easing_bars, float)[ch],
                              dbfs_raw, 0.5);
            if (isinf(dbfs) || isnan(dbfs) || dbfs < -100)
                dbfs = -99.0f;
            ARR_AS(state->vu_meter_st.easing_bars, float)[ch] = dbfs;

            float height = 0;
            if (dbfs <= -36.0f)
                height = MATH_RESCALE(dbfs, -90.0f, -40.0f, 0.0f, lower);
            else
                height = MATH_RESCALE(dbfs, -36.0f, state->vu_meter_st.high_end,
                                      state->vu_meter_st.high_start, size.y);

            height = MATH_CLAMP(height, 0, size.y - 1);

            if (state->opt.vu_meter.render_peak)
            {
                if (height > ARR_AS(state->vu_meter_st.peaks, float)[ch])
                {
                    ARR_AS(state->vu_meter_st.peaks, float)[ch] = height;
                    ARR_AS(state->vu_meter_st.peak_set, uint64_t)
                    [ch] = gclock_now_ns();
                }
                else if (gclock_now_ns() -
                             ARR_AS(state->vu_meter_st.peak_set, uint64_t)[ch] >
                         MS2NS(500))
                {
                    ARR_AS(state->vu_meter_st.peaks, float)
                    [ch] *= state->opt.vu_meter.peak_decay;
                }
            }

            float old_h = ARR_AS(state->vu_meter_st.prev_bars, float)[ch];
            int old_height = (int)old_h;
            int new_height = (int)height;
            float frac = height - new_height;
            int x = pos.x + state->opt.vu_meter.left_pad +
                    ch * (state->opt.vu_meter.bar_width +
                          state->opt.vu_meter.bar_gap);

            if (new_height > old_height)
            {
                for (int y = old_height; y <= new_height; y++)
                {
                    term_draw_pos(buf, VEC(x, pos.y - y));
                    switch (state->opt.vu_meter.style)
                    {
                    case VU_METER_ANALOG_LINE:
                        term_draw_color(
                            buf, COLOR_NONE,
                            (y >= state->vu_meter_st.mark_red
                                 ? GET_THEMECOLOR(state, "VU_METER_HIGH_FG")
                             : y >= state->vu_meter_st.mark_yellow
                                 ? GET_THEMECOLOR(state, "VU_METER_MID_FG")
                                 : GET_THEMECOLOR(state, "VU_METER_LOW_FG")));
                        str_repeat_wchar(buf, L'—',
                                         state->opt.vu_meter.bar_width, NULL);
                        break;
                    case VU_METER_ANALOG_HALF:
                        term_draw_color(
                            buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                            (y >= state->vu_meter_st.mark_red
                                 ? GET_THEMECOLOR(state, "VU_METER_HIGH_FG")
                             : y >= state->vu_meter_st.mark_yellow
                                 ? GET_THEMECOLOR(state, "VU_METER_MID_FG")
                                 : GET_THEMECOLOR(state, "VU_METER_LOW_FG")));
                        str_repeat_wchar(buf, L'▄',
                                         state->opt.vu_meter.bar_width, NULL);
                        break;
                    case VU_METER_ANALOG_BAR:
                        term_draw_color(
                            buf,
                            (y >= state->vu_meter_st.mark_red
                                 ? GET_THEMECOLOR(state, "VU_METER_HIGH_FG")
                             : y >= state->vu_meter_st.mark_yellow
                                 ? GET_THEMECOLOR(state, "VU_METER_MID_FG")
                                 : GET_THEMECOLOR(state, "VU_METER_LOW_FG")),
                            GET_THEMECOLOR(state, "VU_METER_SEP"));
                        if (state->opt.vu_meter.render_separator)
                            str_repeat_wchar(
                                buf, L'—', state->opt.vu_meter.bar_width, NULL);
                        else
                            term_draw_hline(buf, state->opt.vu_meter.bar_width);
                        break;
                    default:
                        break;
                    }
                    term_draw_reset(buf);
                }
            }
            else if (new_height < old_height)
            {
                for (int y = MATH_MIN(old_height + 1, size.y - 1);
                     y > new_height; y--)
                {
                    term_draw_pos(buf, VEC(x, pos.y - y));
                    term_draw_color(
                        buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        (y >= state->vu_meter_st.mark_red
                             ? GET_THEMECOLOR(state, "VU_METER_HIGH_BG")
                         : y >= state->vu_meter_st.mark_yellow
                             ? GET_THEMECOLOR(state, "VU_METER_MID_BG")
                             : GET_THEMECOLOR(state, "VU_METER_LOW_BG")));
                    if (state->opt.vu_meter.render_marks)
                        str_repeat_wchar(buf, L'—',
                                         state->opt.vu_meter.bar_width, NULL);
                    else
                        term_draw_padding(buf, state->opt.vu_meter.bar_width);
                    term_draw_reset(buf);
                }
            }

            if (state->opt.vu_meter.style != VU_METER_ANALOG_LINE &&
                !state->opt.vu_meter.discrete && frac >= 1.0f / 8.0f)
            {
                int y = new_height + 1;
                term_draw_pos(buf, VEC(x, pos.y - y));
                term_draw_color(
                    buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                    (y >= state->vu_meter_st.mark_red
                         ? GET_THEMECOLOR(state, "VU_METER_HIGH_FG")
                     : y >= state->vu_meter_st.mark_yellow
                         ? GET_THEMECOLOR(state, "VU_METER_MID_FG")
                         : GET_THEMECOLOR(state, "VU_METER_LOW_FG")));
                for (int w = 0; w < state->opt.vu_meter.bar_width; ++w)
                    term_draw_vblockf(buf, frac);
                term_draw_reset(buf);
            }

            if (state->opt.vu_meter.render_peak)
            {
                int peak_y = ceilf(ARR_AS(state->vu_meter_st.peaks, float)[ch]);
                if (peak_y + 1 <= size.y && !MATH_AROUND(peak_y, height, 1.0))
                {
                    term_draw_pos(buf, VEC(x, pos.y - peak_y));
                    term_draw_color(
                        buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        (peak_y >= state->vu_meter_st.mark_red
                             ? GET_THEMECOLOR(state, "VU_METER_HIGH_FG")
                         : peak_y >= state->vu_meter_st.mark_yellow
                             ? GET_THEMECOLOR(state, "VU_METER_MID_FG")
                             : GET_THEMECOLOR(state, "VU_METER_LOW_FG")));
                    str_repeat_wchar(buf, L'—', state->opt.vu_meter.bar_width,
                                     NULL);

                    int y = peak_y + 1;
                    term_draw_pos(buf, VEC(x, pos.y - y));
                    term_draw_color(
                        buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        (y >= state->vu_meter_st.mark_red
                             ? GET_THEMECOLOR(state, "VU_METER_HIGH_BG")
                         : y >= state->vu_meter_st.mark_yellow
                             ? GET_THEMECOLOR(state, "VU_METER_MID_BG")
                             : GET_THEMECOLOR(state, "VU_METER_LOW_BG")));

                    if (state->opt.vu_meter.render_marks)
                        str_repeat_wchar(buf, L'—',
                                         state->opt.vu_meter.bar_width, NULL);
                    else
                        term_draw_hline(buf, state->opt.vu_meter.bar_width);
                }
                else
                {
                    int y = height + 2;
                    term_draw_pos(buf, VEC(x, pos.y - y));
                    term_draw_color(
                        buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                        (y >= state->vu_meter_st.mark_red
                             ? GET_THEMECOLOR(state, "VU_METER_HIGH_BG")
                         : y >= state->vu_meter_st.mark_yellow
                             ? GET_THEMECOLOR(state, "VU_METER_MID_BG")
                             : GET_THEMECOLOR(state, "VU_METER_LOW_BG")));
                    if (state->opt.vu_meter.render_marks)
                        str_repeat_wchar(buf, L'—',
                                         state->opt.vu_meter.bar_width, NULL);
                    else
                        term_draw_hline(buf, state->opt.vu_meter.bar_width);
                }
            }

            if (state->opt.vu_meter.render_numeric)
            {
                term_draw_pos(buf, VEC(x, pos.y + 1));
                term_draw_color(buf, GET_THEMECOLOR(state, "VU_METER_BG"),
                                GET_THEMECOLOR(state, "VU_METER_FG"));
                str_catf(buf, "%-4.0f", dbfs);
                term_draw_reset(buf);
            }

            ARR_AS(state->vu_meter_st.prev_bars, float)[ch] = height;
        }
    }

    term_draw_reset(buf);
}
