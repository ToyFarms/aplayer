#include "widgets.h"

int render_volume(ui_state *state, vec2 pos, vec2 size, float gain)
{
    str_t *buf = &state->term->buf;

    term_draw_pos(buf, pos);
    term_draw_color(buf, GET_THEMECOLOR(state, "VOLUME_BG"),
                    GET_THEMECOLOR(state, "VOLUME_FG"));
    term_draw_padding(buf, size.x);

    term_draw_pos(buf, pos);
    size_t pre = buf->len;
    term_draw_strf(buf, "%s%.1f dB", gain > 0.0f ? "+" : "", gain);
    size_t len = buf->len - pre;

    term_draw_reset(buf);

    return len;
}
