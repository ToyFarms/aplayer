#include "widgets.h"

static int get_line_state(ui_state *state, int idx)
{
    int ret = PLAYLIST_LINE_NOT_INITIALIZED;

    if (idx == state->app->playlist.current_idx)
        ret |= PLAYLIST_LINE_PLAYING;
    if (idx == state->playlist_st.hovered_idx)
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

void render_list(ui_state *state, vec2 pos, vec2 size)
{
    bool redraw = state->term->resized || state->playlist_st.redraw;
    if (state->playlist_st.lines.data == NULL)
        state->playlist_st.lines =
            array_create(MATH_MAX(state->app->playlist.files.length, size.y),
                         sizeof(int32_t));
    else if (state->playlist_st.lines.capacity <
             state->app->playlist.files.length)
    {
        array_resize(&state->playlist_st.lines,
                     state->app->playlist.files.length);
        redraw = true;
    }

    state->playlist_st.hovered_idx =
        MATH_CLAMP(state->playlist_st.hovered_idx, 0,
                   state->app->playlist.files.length - 1);

    int leftover = MATH_MAX(state->app->playlist.files.length - size.y, 0);

    int prev_offset = state->playlist_st.viewport_offset;

    if (state->playlist_st.hovered_idx - state->playlist_st.viewport_offset >
        size.y - state->opt.scrolloff)
    {
        state->playlist_st.viewport_offset =
            MATH_CLAMP(state->playlist_st.viewport_offset +
                           ((state->playlist_st.hovered_idx -
                             state->playlist_st.viewport_offset) -
                            (size.y - state->opt.scrolloff)),
                       0, leftover);
    }
    else if (state->playlist_st.hovered_idx -
                 state->playlist_st.viewport_offset <
             state->opt.scrolloff)
    {
        state->playlist_st.viewport_offset = MATH_MAX(
            state->playlist_st.viewport_offset -
                (state->opt.scrolloff - (state->playlist_st.hovered_idx -
                                         state->playlist_st.viewport_offset)) +
                1,
            0);
    }

    if (state->playlist_st.recenter)
    {
        state->playlist_st.viewport_offset =
            state->playlist_st.hovered_idx - size.y / 2;
        state->playlist_st.recenter = false;
    }

    state->playlist_st.viewport_offset =
        MATH_CLAMP(state->playlist_st.viewport_offset, 0, leftover);

    if (prev_offset != state->playlist_st.viewport_offset)
        redraw = true;

    str_t *buf = &state->term->buf;
    for (int i = 0; i < size.y && i + state->playlist_st.viewport_offset <
                                      state->app->playlist.files.length;
         i++)
    {
        int abs_idx = i + state->playlist_st.viewport_offset;
        int32_t line_state = get_line_state(state, abs_idx);
        if (!redraw &&
            line_state == ARR_AS(state->playlist_st.lines, int)[abs_idx])
            continue;

        ARR_AS(state->playlist_st.lines, int)[abs_idx] = line_state;
        str_t line = str_create();
        str_catch(&line, ' ');

        term_draw_pos(buf, VEC(pos.x, pos.y + i));
        term_draw_color(buf, GET_THEMECOLOR(state, "LIST_NUMBER_BG"),
                        GET_THEMECOLOR(state, "LIST_NUMBER_FG"));
        int left = size.x - pos.x;
        size_t pre = buf->len;
        str_catf(buf, "%5d ", abs_idx);
        left -= buf->len - pre;
        term_draw_reset(buf);

        style_line_start(state, buf, line_state);

        fs_entry_t *entry =
            playlist_get_at_index(&state->app->playlist, abs_idx);
        str_cat(&line, entry->name.buf);

        size_t width = term_draw_truncate(buf, &line, left);
        term_draw_padding(buf, left - width);

        str_free(&line);

        style_line_end(state, buf, line_state);
    }

    term_draw_reset(buf);
    state->playlist_st.redraw = false;
}
