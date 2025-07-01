#include "widgets.h"

void render_rect(ui_state *state, vec2 pos, vec2 size, color_t color)
{
    str_t *buf = &state->term->buf;

    term_draw_pos(buf, pos);
    term_draw_rect(buf, size, color, COLOR_NONE);
    term_draw_pos(buf, pos);
    term_draw_strf(buf, "%d:%d", pos.x, size.x);
}
