#include "widgets.h"
#include <math.h>

void format_timestamp(uint64_t us, str_t *out)
{
    uint64_t seconds = fmod((double)us / 1e6, 60.0);
    uint64_t minutes = fmod((double)us / (1e6 * 60.0), 60.0);
    uint64_t hours = fmod((double)us / (1e6 * 3600.0), 24.0);
    uint64_t days = floor((double)us / (1e6 * 3600.0 * 24.0));

    if (days >= 1)
        str_catf(out, "%01lu:", days);

    bool have_hour = hours >= 1 || days >= 1;
    if (have_hour)
        str_catf(out, days >= 1 ? "%02lu:" : "%01lu:", hours);

    str_catf(out, have_hour ? "%02lu:%02lu" : "%01lu:%02lu", minutes, seconds);
}

int render_timestamp(ui_state *state, vec2 pos, vec2 size, uint64_t timestamp,
                     uint64_t duration)
{
    str_t *buf = &state->term->buf;

    term_draw_pos(buf, pos);

    term_draw_color(buf, GET_THEMECOLOR(state, "TIMESTAMP_BG"),
                    GET_THEMECOLOR(state, "TIMESTAMP_FG"));

    size_t pre = buf->len;
    format_timestamp(timestamp, buf);
    str_cat(buf, " / ");
    format_timestamp(duration, buf);
    size_t len = buf->len - pre;

    term_draw_reset(buf);

    return len;
}
