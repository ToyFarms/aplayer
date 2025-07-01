#include "widgets.h"
#include <stdlib.h>
#include <string.h>

// TODO: make this actually good
void render_debug(ui_state *state, vec2 pos, vec2 size)
{
    str_t *buf = &state->term->buf;
    char *line = NULL;
    static int i = 0;
    while ((line = queue_pop(&state->debug_st.logs)))
    {
        int row = state->debug_st.current_row;
        term_draw_pos(buf, VEC(pos.x, pos.y + row));
        term_draw_hline(buf, size.x);
        term_draw_pos(buf, VEC(pos.x, pos.y + row));
        term_draw_strf(buf, "%5d: %s", i++, line);

        state->debug_st.current_row += 1;
        state->debug_st.current_row %= size.y;

        free(line);
    }
}
