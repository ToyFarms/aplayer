#include "clock.h"
#include "utils.h"
#include "widgets.h"

static void change_state(ui_state *state, int s)
{
    static uint64_t last_update = 0;
    uint64_t now = gclock_now_ns();
    if (now - last_update < MS2NS(200))
        return;
    last_update = now;
    switch (s)
    {
    case 0:
        play_prev(state->app);
        break;
    case 1:
        state->app->audio->mixer.paused = !state->app->audio->mixer.paused;
        break;
    case 2:
        play_next(state->app);
        break;
    }
}

void render_media_control(ui_state *state, vec2 pos, vec2 size)
{
    str_t *buf = &state->term->buf;

    if (state->term->mouse_y == pos.y && state->term->mouse_x >= pos.x &&
        state->term->mouse_x < pos.x + size.x)
    {
        if (state->term->mouse_x >= pos.x && state->term->mouse_x < pos.x + 4)
        {
            state->media_ctl_st.prev_hovered = true;
            state->media_ctl_st.prev_last = 1;
            if (state->term->click[0])
                change_state(state, 0);
        }
        else if (state->term->mouse_x >= pos.x + 6 &&
                 state->term->mouse_x < pos.x + 11)
        {
            state->media_ctl_st.play_hovered = true;
            state->media_ctl_st.play_last = 1;
            if (state->term->click[0])
                change_state(state, 1);
        }
        else if (state->term->mouse_x >= pos.x + 13 &&
                 state->term->mouse_x < pos.x + 17)
        {
            state->media_ctl_st.next_hovered = true;
            state->media_ctl_st.next_last = 1;
            if (state->term->click[0])
                change_state(state, 2);
        }
    }

    term_draw_pos(buf, pos);

    if (state->media_ctl_st.prev_hovered)
        term_draw_color(buf, GET_THEMECOLOR(state, "MEDIA_CONTROL_BG"),
                        GET_THEMECOLOR(state, "MEDIA_CONTROL_FG"));
    str_catwcs(buf, L"  <<  ");
    if (state->media_ctl_st.prev_hovered)
        term_draw_reset(buf);

    term_draw_padding(buf, 2);

    if (state->media_ctl_st.play_hovered)
        term_draw_color(buf, GET_THEMECOLOR(state, "MEDIA_CONTROL_BG"),
                        GET_THEMECOLOR(state, "MEDIA_CONTROL_FG"));
    if (state->app->audio->mixer.paused ||
        state->app->audio->mixer.sources.length == 0)
        str_catwcs(buf, L"  |>  ");
    else
        str_catwcs(buf, L"  ||  ");
    if (state->media_ctl_st.play_hovered)
        term_draw_reset(buf);

    term_draw_padding(buf, 2);

    if (state->media_ctl_st.next_hovered)
        term_draw_color(buf, GET_THEMECOLOR(state, "MEDIA_CONTROL_BG"),
                        GET_THEMECOLOR(state, "MEDIA_CONTROL_FG"));
    str_catwcs(buf, L"  >>  ");
    if (state->media_ctl_st.next_hovered)
        term_draw_reset(buf);

    if (state->media_ctl_st.prev_last != 0 &&
        (gclock_now_ns() - state->media_ctl_st.prev_last > MS2NS(100)))
    {
        state->media_ctl_st.prev_hovered = false;
        state->media_ctl_st.prev_last = 0;
    }

    if (state->media_ctl_st.play_last != 0 &&
        (gclock_now_ns() - state->media_ctl_st.play_last > MS2NS(100)))
    {
        state->media_ctl_st.play_hovered = false;
        state->media_ctl_st.play_last = 0;
    }

    if (state->media_ctl_st.next_last != 0 &&
        (gclock_now_ns() - state->media_ctl_st.next_last > MS2NS(100)))
    {
        state->media_ctl_st.next_hovered = false;
        state->media_ctl_st.next_last = 0;
    }
}
