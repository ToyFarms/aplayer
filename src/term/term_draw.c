#include "term_draw.h"
#include "term.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

static const wchar_t *blocks_horizontal = L" ▎▎▍▌▋▊▉█";
static const wchar_t *blocks_vertical = L" ▂▂▄▄▅▆▇█";
static const int blocks_len = 9;

string_t *term_draw_pos(string_t *buf, vec2 pos)
{
    return string_catf_d(buf, TESC TPOSYX, pos.y + 1, pos.x + 1);
}

string_t *term_draw_move(string_t *buf, vec2 pos)
{
    if (pos.y > 0)
        buf = string_catf_d(buf, TESC TNDOWN, pos.y);
    else if (pos.y < 0)
        buf = string_catf_d(buf, TESC TNUP, pos.y * -1);
    if (pos.x > 0)
        buf = string_catf_d(buf, TESC TNRIGHT, pos.x);
    else if (pos.x < 0)
        buf = string_catf_d(buf, TESC TNLEFT, pos.x * -1);
    return buf;
}

string_t *term_draw_color(string_t *buf, color_t bg, color_t fg)
{
    if (bg.a != 0 && fg.a != 0)
        return string_catf_d(buf, TESC TBGFG, bg.r, bg.g, bg.b, fg.r, fg.g,
                            fg.b);
    else if (bg.a != 0)
        return string_catf_d(buf, TESC TBG, bg.r, bg.g, bg.b);
    else if (fg.a != 0)
        return string_catf_d(buf, TESC TFG, fg.r, fg.g, fg.b);
    else
        return buf;
}

string_t *term_draw_str(string_t *buf, const char *str, int len)
{
    return len > 0 ? string_catlen(buf, str, len) : string_cat(buf, str);
}

string_t *term_draw_strf(string_t *buf, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    buf = string_catfv(buf, fmt, args);
    va_end(args);
    return buf;
}

string_t *term_draw_padding(string_t *buf, int length)
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
        buf = string_catlen(buf, padding, chunk);
        length -= chunk;
    }

    return buf;
}

string_t *term_draw_hline(string_t *buf, int length)
{
    if (length <= 0)
        return buf;
    return string_catf_d(buf, TESC THLINE, length);
}

string_t *term_draw_vline(string_t *buf, int length)
{
    for (int i = 0; i < length; i++)
        buf = string_cat(buf, " " TESC TDOWN TESC TLEFT);
    return buf;
}

string_t *term_draw_hblockf(string_t *buf, float x)
{
    x = MATH_CLAMP(x, 0.0f, 1.0f);
    int index = x * (blocks_len - 1);

    return string_catwch(buf, blocks_horizontal[index]);
}

string_t *term_draw_vblockf(string_t *buf, float x)
{
    x = MATH_CLAMP(x, 0.0f, 1.0f);
    int index = x * (blocks_len - 1);

    return string_catwch(buf, blocks_vertical[index]);
}

string_t *term_draw_rect(string_t *buf, vec2 size, color_t bg, color_t fg)
{
    for (int i = 0; i < size.y; i++)
    {
        buf = term_draw_color(buf, bg, fg);
        buf = string_catf_d(buf, TESC THLINE TESC TDOWN, size.x);
    }
    return buf;
}
