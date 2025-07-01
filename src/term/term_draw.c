#include "term_draw.h"
#include "term.h"
#include "wcwidth.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <wchar.h>

static const wchar_t blocks_horizontal[] = {
    L' ', L'▏', L'▎', L'▍', L'▌', L'▋', L'▊', L'▉', L'█',
};
static const wchar_t *blocks_vertical = L" ▁▂▃▄▅▆▇█";
static const int blocks_len = 9;
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
    else if (color_mode == TERM_COLOR_256)
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
    else if (color_mode == TERM_COLOR_MONO)
    {
        // TODO: support this, probably will have severe readibility issues, but
        // whatever
    }

    return buf;
}

str_t *term_draw_reset(str_t *buf)
{
    return str_cat(buf, TESC TRESET);
}

str_t *term_draw_clear(str_t *buf)
{
    return str_cat(buf, TESC TCLEAR);
}

int term_draw_strwidth(str_t *buf)
{
    wchar_t *utf16 = str_decode(buf);
    int size = mk_wcswidth(utf16, wcslen(utf16));
    free(utf16);

    return size;
}

size_t term_draw_truncate(str_t *dst, str_t *buf, size_t width)
{
    mbstate_t st = {0};
    const char *p = buf->buf;
    size_t used = 0;
    size_t total_bytes = 0;

    while (*p)
    {
        wchar_t wc;
        size_t clen = mbrtowc(&wc, p, MB_CUR_MAX, &st);
        if (clen == (size_t)-2)
        {
            clen = 1;
            wc = L'?';
            memset(&st, 0, sizeof st);
        }
        else if (clen == (size_t)-1)
        {
            clen = 1;
            wc = L'?';
            memset(&st, 0, sizeof st);
        }
        else if (clen == 0)
        {
            break;
        }

        int w = mk_wcwidth(wc);
        if (w < 0)
            w = 0;

        if (used + (size_t)w > width)
        {
            break;
        }

        used += (size_t)w;
        total_bytes += clen;
        p += clen;
    }

    str_cat_strlen(dst, buf, total_bytes);

    return used;
}

size_t term_draw_truncate_termchar(str_t *dst, str_t *buf, wchar_t end_char,
                                   size_t width)
{
    int ell_w = mk_wcwidth(end_char);
    if (ell_w < 0)
        ell_w = 0;

    mbstate_t st = {0};
    const char *p = buf->buf;
    size_t used = 0;
    size_t total_bytes = 0;
    int truncated = 0;

    size_t reserve = (width > (size_t)ell_w ? (size_t)ell_w : width);
    size_t limit = width - reserve;

    while (*p)
    {
        wchar_t wc;
        size_t clen = mbrtowc(&wc, p, MB_CUR_MAX, &st);
        if (clen == (size_t)-2)
        {
            clen = 1;
            wc = L'?';
            memset(&st, 0, sizeof st);
        }
        else if (clen == (size_t)-1)
        {
            clen = 1;
            wc = L'?';
            memset(&st, 0, sizeof st);
        }
        else if (clen == 0)
        {
            break;
        }

        int w = mk_wcwidth(wc);
        if (w < 0)
            w = 0;

        if (used + (size_t)w > limit)
        {
            truncated = 1;
            break;
        }

        used += (size_t)w;
        total_bytes += clen;
        p += clen;
    }

    str_cat_strlen(dst, buf, total_bytes);

    if (truncated)
    {
        str_catwch(dst, end_char);
        used += (size_t)ell_w;
    }

    return used;
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

#define MAX_PAD 1024
    static thread_local char pad[MAX_PAD];
    static thread_local bool initialized = false;
    if (!initialized)
    {
        memset(pad, ' ', MAX_PAD);
        initialized = true;
    }

    while (length > MAX_PAD)
    {
        str_catlen(buf, pad, MAX_PAD);
        length -= MAX_PAD;
    }
    str_catlen(buf, pad, length);
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

str_t *term_draw_hlinef(str_t *buf, float length, int max_length, color_t line,
                        color_t bg)
{
    if (length <= 0)
        return buf;

    int whole = (int)length;
    float rem = length - whole;

    term_draw_color(buf, line, COLOR_NONE);
    term_draw_padding(buf, whole);

    term_draw_color(buf, bg, line);
    term_draw_hblockf(buf, rem);

    term_draw_padding(buf, max_length - (whole + 1));

    term_draw_reset(buf);

    return buf;
}

str_t *term_draw_hblockf(str_t *buf, float x)
{
    x = fmodf(x, 1.0f);
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
    if (size.x <= 0 || size.y <= 0)
        return buf;

    // TODO: compute color only once
    for (int i = 0; i < size.y; i++)
    {
        term_draw_color(buf, bg, fg);
        str_catf_d(buf, TESC THLINE TESC TRESET TESC TDOWN, size.x);
    }
    return buf;
}

str_t *term_draw_invert(str_t *buf)
{
    return str_cat(buf, TESC TNEGATIVE);
}
