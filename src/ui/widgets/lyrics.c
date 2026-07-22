#include "_math.h"
#include "audio_source.h"
#include "clock.h"
#include "logger.h"
#include "metagen.h"
#include "ui.h"
#include "widgets.h"

static int get_line_state(ui_state *state, int idx)
{
    int ret = PLAYLIST_LINE_NOT_INITIALIZED;

    // if (idx == state->app->playlist.current_idx)
    //     ret |= PLAYLIST_LINE_PLAYING;
    if (idx == state->lyrics_st.hovered_idx)
        ret |= PLAYLIST_LINE_HOVERED;

    return ret;
}

static void style_line_start(ui_state *state, str_t *buf, int32_t line)
{
    if (PLAYLIST_IS_LINE_NORMAL(line))
    {
        term_draw_color(buf, GET_THEMECOLOR(state, "LIST_NORMAL_BG"),
                        GET_THEMECOLOR(state, "LIST_NORMAL_FG"));
        return;
    }

    if (line & PLAYLIST_LINE_PLAYING)
    {
        term_draw_color(buf, GET_THEMECOLOR(state, "LIST_PLAYING_BG"),
                        GET_THEMECOLOR(state, "LIST_PLAYING_FG"));
    }
    else if (line & PLAYLIST_LINE_HOVERED)
        term_draw_invert(buf);
}

static void style_line_end(ui_state *state, str_t *buf, int32_t line)
{
    if (line & PLAYLIST_LINE_PLAYING || line & PLAYLIST_LINE_HOVERED)
        term_draw_reset(buf);
}

void render_lyrics(ui_state *state, vec2 pos, vec2 size)
{
    audio_source *src =
        &ARR_AS(state->app->audio->mixer.sources, audio_source)[0];

    if (src->get_metadata == NULL)
        return;

    metadata_t *meta = src->get_metadata(src);
    if (meta == NULL)
        return;

    size_t n_lyrics = 0;
    if (meta->n_lyrics_synced > 0)
        n_lyrics = meta->n_lyrics_synced;
    else
        n_lyrics = meta->n_lyrics;

    // TODO: this should not hard return, let it redraw
    if (n_lyrics == 0)
        return;

    bool redraw = state->term->resized || state->lyrics_st.redraw;
    if (state->lyrics_st.lines.data == NULL)
        state->lyrics_st.lines =
            array_create(MATH_MAX(n_lyrics, size.y), sizeof(int32_t));
    else if (state->lyrics_st.lines.capacity < n_lyrics)
    {
        array_resize(&state->lyrics_st.lines, n_lyrics);
        redraw = true;
    }
    int leftover = MATH_MAX(n_lyrics - size.y, 0);
    int prev_offset = state->lyrics_st.viewport_offset;

    if (state->lyrics_st.hovered_idx - state->lyrics_st.viewport_offset >
        size.y - state->opt.scrolloff)
    {
        state->lyrics_st.viewport_offset =
            MATH_CLAMP(state->lyrics_st.viewport_offset +
                           ((state->lyrics_st.hovered_idx -
                             state->lyrics_st.viewport_offset) -
                            (size.y - state->opt.scrolloff)),
                       0, leftover);
    }
    else if (state->lyrics_st.hovered_idx - state->lyrics_st.viewport_offset <
             state->opt.scrolloff)
    {
        state->lyrics_st.viewport_offset = MATH_MAX(
            state->lyrics_st.viewport_offset -
                (state->opt.scrolloff - (state->lyrics_st.hovered_idx -
                                         state->lyrics_st.viewport_offset)) +
                1,
            0);
    }

    // if (state->lyrics_st.recenter)
    // {
    state->lyrics_st.viewport_offset =
        state->lyrics_st.hovered_idx - size.y / 2;
    state->lyrics_st.recenter = false;
    // }

    state->lyrics_st.viewport_offset =
        MATH_CLAMP(state->lyrics_st.viewport_offset, 0, leftover);

    if (prev_offset != state->lyrics_st.viewport_offset)
        redraw = true;

    int64_t ts = US2MS(src->timestamp);

    str_t *buf = &state->term->buf;
    if (meta->n_lyrics_synced > 0)
    {
        int run = 0;
        for (int i = 0;
             i < size.y && i + state->lyrics_st.viewport_offset < n_lyrics; i++)
        {
            run = 1;
            int abs_idx = i + state->lyrics_st.viewport_offset;

            int start = meta->lyrics_synced[abs_idx].start_ms;
            int end = start + 2000;
            if (abs_idx < n_lyrics - 2)
                end = meta->lyrics_synced[abs_idx + 1].start_ms;

            int32_t line_state = get_line_state(state, abs_idx);

            if (MATH_INBETWEEN(ts, start, end))
            {
                line_state |= PLAYLIST_LINE_PLAYING;
                state->lyrics_st.hovered_idx = abs_idx;
            }
            else
                line_state &= ~PLAYLIST_LINE_PLAYING;
            state->lyrics_st.recenter = true;

            if (!redraw &&
                line_state == ARR_AS(state->lyrics_st.lines, int)[abs_idx])
                continue;
            ARR_AS(state->lyrics_st.lines, int)[abs_idx] = line_state;

            term_draw_pos(buf, VEC(pos.x, pos.y + i));

            style_line_start(state, buf, line_state);

            size_t width = term_draw_truncate(
                buf, meta->lyrics_synced[abs_idx].text, size.x);
            term_draw_padding(buf, size.x - width);

            style_line_end(state, buf, line_state);
        }

        if (run == 0)
        {
            state->lyrics_st.recenter = true;
        }
    }
    else if (meta->n_lyrics > 0)
    {
        for (size_t i = 0; i < meta->n_lyrics; ++i)
        {
            term_draw_strf(buf, "%s", meta->lyrics[i].text);
        }
    }
    else
    {
        term_draw_str(buf, "no lyrics", -1);
    }

    term_draw_reset(buf);
    state->playlist_st.redraw = false;
}
