#include "widgets.h"
#include <math.h>

void render_hprogress(ui_state *state, vec2 pos, vec2 size, float progress)
{
    str_t *buf = &state->term->buf;
    term_draw_pos(buf, pos);
    progress *= size.x;

    if (size.y == 1)
    {
        if (isnan(progress))
        {
            term_draw_color(buf, GET_THEMECOLOR(state, "PROGRESS_BG"),
                            GET_THEMECOLOR(state, "PROGRESS_FG"));
            term_draw_hline(buf, size.x);
        }
        else
        {
            term_draw_hlinef(buf, progress, size.x,
                             GET_THEMECOLOR(state, "PROGRESS_FG"),
                             GET_THEMECOLOR(state, "PROGRESS_BG"));
        }
    }
    else
    {
        term_draw_str(buf, "HPROGRESS WITH HEIGHT > 1 NOT IMPLEMENTED", -1);
    }

    term_draw_reset(buf);
}
