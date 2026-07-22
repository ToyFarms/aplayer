#include "widgets.h"

int render_volume(ui_state *state, vec2 pos, vec2 size, float gain)
{
    if (state->app->audio->mixer.muted)
    {
        term_draw_str(&state->term->buf, TESC TSTRIKETHROUGH, -1);
        return render_volume_color(state, pos, size, gain,
                                   GET_THEMECOLOR(state, "VOLUME_BG_MUTED"),
                                   GET_THEMECOLOR(state, "VOLUME_FG_MUTED"));
    }

    return render_volume_color(state, pos, size, gain,
                               GET_THEMECOLOR(state, "VOLUME_BG"),
                               GET_THEMECOLOR(state, "VOLUME_FG"));
}

int render_volume_color(ui_state *state, vec2 pos, vec2 size, float gain,
                        color_t bg, color_t fg)
{
    str_t *buf = &state->term->buf;

    term_draw_pos(buf, pos);
    term_draw_color(buf, bg, fg);
    term_draw_padding(buf, size.x);

    term_draw_pos(buf, pos);
    size_t pre = buf->len;
    term_draw_strf(buf, "%s%.1f dB", gain > 0.0f ? "+" : "", gain);
    size_t len = buf->len - pre;

    term_draw_reset(buf);

    return len;
}
