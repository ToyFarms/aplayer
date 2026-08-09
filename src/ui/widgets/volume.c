#include "term_draw.h"
#include "widgets.h"

int render_volume(ui_state *state, vec2 pos, vec2 size, float gain,
                  const char *label)
{
    if (state->app->audio->mixer.muted)
    {
        term_draw_str(&state->term->buf, TESC TSTRIKETHROUGH, -1);
        return render_volume_color(state, pos, size, gain, label,
                                   GET_THEMECOLOR(state, "VOLUME_BG_MUTED"),
                                   GET_THEMECOLOR(state, "VOLUME_FG_MUTED"));
    }

    return render_volume_color(state, pos, size, gain, label,
                               GET_THEMECOLOR(state, "VOLUME_BG"),
                               GET_THEMECOLOR(state, "VOLUME_FG"));
}

int render_volume_color(ui_state *state, vec2 pos, vec2 size, float gain,
                        const char *label, color_t bg, color_t fg)
{
    str_t *buf = &state->term->buf;

    term_draw_pos(buf, pos);
    term_draw_color(buf, bg, fg);

    term_draw_pos(buf, pos);
    size_t pre = buf->len;
    term_draw_strf(buf, "%s%s%s%.1f dB", label != NULL ? label : "",
                   label != NULL ? " " : "", gain > 0.0f ? "+" : "", gain);
    size_t len = buf->len - pre;
    term_draw_rect(buf, VEC(size.x - len, 1), COLOR_NONE, COLOR_NONE);

    term_draw_reset(buf);

    return len;
}
