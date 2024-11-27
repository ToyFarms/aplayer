#include "term_draw.h"
#include "term.h"

#include <string.h>
#include <wchar.h>

static const wchar_t *blocks_horizontal = L" ▏▎▍▌▋▊▉█";
static const wchar_t *blocks_vertical = L" ▁▂▃▄▅▆▇█";
static const int blocks_len = 10;
static enum term_color_mode color_mode = TERM_COLOR_24BIT;

void term_color_mode(enum term_color_mode mode)
{
    color_mode = mode;
}

enum term_color_mode term_get_color_mode()
{
    return color_mode;
}

str_t *term_draw_pos(str_t *buf, vec2 pos)
{
    return str_catf_d(buf, TESC TPOSYX, pos.y + 1, pos.x + 1);
}

str_t *term_draw_move(str_t *buf, vec2 pos)
{
    if (pos.y > 0)
        str_catf_d(buf, TESC TNDOWN, pos.y);
    else if (pos.y < 0)
        str_catf_d(buf, TESC TNUP, pos.y * -1);
    if (pos.x > 0)
        str_catf_d(buf, TESC TNRIGHT, pos.x);
    else if (pos.x < 0)
        str_catf_d(buf, TESC TNLEFT, pos.x * -1);
    return buf;
}

str_t *term_draw_color(str_t *buf, color_t bg, color_t fg)
{
    if (color_mode == TERM_COLOR_24BIT)
    {
        if (bg.a != 0 && fg.a != 0)
            return str_catf_d(buf, TESC TBGFG, bg.r, bg.g, bg.b, fg.r, fg.g,
                                 fg.b);
        else if (bg.a != 0)
            return str_catf_d(buf, TESC TBG, bg.r, bg.g, bg.b);
        else if (fg.a != 0)
            return str_catf_d(buf, TESC TFG, fg.r, fg.g, fg.b);
        else
            return buf;
    }
    else
    {
        if (bg.a != 0 && fg.a != 0)
            return str_catf_d(buf, TESC TBGFG8, color_term_pallete(bg),
                                 color_term_pallete(fg));
        else if (bg.a != 0)
            return str_catf_d(buf, TESC TBG8, color_term_pallete(bg));
        else if (fg.a != 0)
            return str_catf_d(buf, TESC TFG8, color_term_pallete(fg));
        else
            return buf;
    }
}

str_t *term_draw_str(str_t *buf, const char *str, int len)
{
    return len > 0 ? str_catlen(buf, str, len) : str_cat(buf, str);
}

str_t *term_draw_strf(str_t *buf, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    str_catfv(buf, fmt, args);
    va_end(args);
    return buf;
}

str_t *term_draw_padding(str_t *buf, int length)
{
    if (length <= 0)
        return buf;

#define MAX_PADDING 1024
    static char padding[MAX_PADDING];
    static bool initialized = false;
    if (!initialized)
    {
        memset(padding, ' ', MAX_PADDING);
        initialized = true;
    }

    while (length > 0)
    {
        int chunk = length < MAX_PADDING ? length : MAX_PADDING;
        str_catlen(buf, padding, chunk);
        length -= chunk;
    }

    return buf;
}

str_t *term_draw_hline(str_t *buf, int length)
{
    if (length <= 0)
        return buf;
    return str_catf_d(buf, TESC THLINE, length);
}

str_t *term_draw_vline(str_t *buf, int length, color_t bg, color_t fg)
{
    // NOTE: will crash if color length exceed 128; it shouldn't, but who knows
    char c[128];
    str_t cs = {.buf = c, .len = 0, .capacity = 128};
    term_draw_color(&cs, bg, fg);

    for (int i = 0; i < length; i++)
    {
        str_catlen(buf, c, cs.len);
        str_cat(buf, " " TESC TDOWN TESC TLEFT TESC TRESET);
    }
    return buf;
}

// TODO: implement hlinef and vlinef

str_t *term_draw_hblockf(str_t *buf, float x)
{
    x = MATH_CLAMP(x, 0.0f, 1.0f);
    int index = x * (blocks_len - 1);

    return str_catwch(buf, blocks_horizontal[index]);
}

str_t *term_draw_vblockf(str_t *buf, float x)
{
    x = MATH_CLAMP(x, 0.0f, 1.0f);
    int index = x * (blocks_len - 1);

    return str_catwch(buf, blocks_vertical[index]);
}

str_t *term_draw_rect(str_t *buf, vec2 size, color_t bg, color_t fg)
{
    // TODO: compute color only once
    for (int i = 0; i < size.y; i++)
    {
        term_draw_color(buf, bg, fg);
        str_catf_d(buf, TESC THLINE TESC TDOWN, size.x);
    }
    return buf;
}
