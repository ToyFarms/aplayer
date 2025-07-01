#include "widgets.h"

int tabs_get_width(ui_state *state)
{
    int width = 0;
    str_t *s;
    ARR_FOREACH_BYREF(state->tabs_st.tabs, s, i)
    {
        width += s->len;
    }

    width += (state->tabs_st.tabs.length - 1) * state->opt.tabs.gap +
             (state->opt.tabs.padding * 2) + (4 * state->tabs_st.tabs.length);
    return width;
}

void render_tabs(ui_state *state, vec2 pos, vec2 size)
{
    str_t *buf = &state->term->buf;
    term_draw_pos(buf, pos);

    str_t *s;
    ARR_FOREACH_BYREF(state->tabs_st.tabs, s, i)
    {
        if (i != 0)
            term_draw_padding(buf, state->opt.tabs.gap);
        bool selected = (enum tab_type)i == state->tabs_st.selected;
        if (selected)
            term_draw_invert(buf);
        term_draw_padding(buf, state->opt.tabs.padding);
        str_catf(buf, "[%d] %s", i + 1, s->buf);
        term_draw_padding(buf, state->opt.tabs.padding);
        if (selected)
            term_draw_reset(buf);
    }
}
