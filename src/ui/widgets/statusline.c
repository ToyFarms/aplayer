#include "widgets.h"

void render_statusline(ui_state *state, vec2 pos, vec2 size)
{
    str_t *buf = &state->term->buf;
    term_draw_pos(buf, pos);

    term_draw_color(buf, GET_THEMECOLOR(state, "STATUSLINE_BG"),
                    GET_THEMECOLOR(state, "STATUSLINE_FG"));
    term_draw_hline(buf, size.x);
    term_draw_pos(buf, pos);

    str_catf(buf, "%s%s/%s", state->app->playlist.is_shuffled ? "*" : "", playlist_sort_name(state->app->playlist.sort),
             playlist_sort_dir_name(state->app->playlist.sort_direction));
}
