#include "tui_internal.h"
#include "wcwidth.h"

/* ------------------------------------------------------------------------ */
/* utf-8 / width                                                            */
/* ------------------------------------------------------------------------ */

size_t tui__utf8_decode(const char *p, size_t len, uint32_t *cp)
{
    if (len == 0)
    {
        *cp = 0;
        return 0;
    }
    unsigned char c = (unsigned char)p[0];
    size_t need;
    uint32_t v;
    if (c < 0x80)
    {
        *cp = c;
        return 1;
    }
    else if ((c & 0xE0) == 0xC0)
    {
        need = 2;
        v = c & 0x1F;
    }
    else if ((c & 0xF0) == 0xE0)
    {
        need = 3;
        v = c & 0x0F;
    }
    else if ((c & 0xF8) == 0xF0)
    {
        need = 4;
        v = c & 0x07;
    }
    else
    {
        *cp = 0xFFFD;
        return 1;
    }
    if (len < need)
    {
        *cp = 0xFFFD;
        return 1;
    }
    for (size_t i = 1; i < need; i++)
    {
        unsigned char cc = (unsigned char)p[i];
        if ((cc & 0xC0) != 0x80)
        {
            *cp = 0xFFFD;
            return 1;
        }
        v = (v << 6) | (cc & 0x3F);
    }
    *cp = v;
    return need;
}

int tui__cp_width(uint32_t cp)
{
    if (cp == '\t')
        return 1;
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0))
        return 0;
    int w = mk_wcwidth(cp);
    return w < 0 ? 0 : (w > 2 ? 2 : w);
}

int tui__text_width(const char *s, size_t len)
{
    int w = 0;
    size_t i = 0;
    while (i < len)
    {
        uint32_t cp;
        i += tui__utf8_decode(s + i, len - i, &cp);
        w += tui__cp_width(cp);
    }
    return w;
}

/* ------------------------------------------------------------------------ */
/* line breaking                                                            */
/* ------------------------------------------------------------------------ */

/* Next visual line of one logical line (no '\n' inside `s`).
 * Returns bytes in the visual line via *line_len / *line_w and how many bytes
 * were consumed (line + the whitespace we broke on) as the return value. */
static size_t wrap_next(const char *s, size_t len, int width, size_t *line_len,
                        int *line_w)
{
    if (width < 1)
        width = 1;

    size_t i = 0;
    int w = 0;
    bool have_break = false;
    size_t break_i = 0;
    int break_w = 0;

    while (i < len)
    {
        uint32_t cp;
        size_t n = tui__utf8_decode(s + i, len - i, &cp);
        int cw = tui__cp_width(cp);

        if (cp == ' ')
        {
            if (w + cw > width)
            {
                /* the space itself does not fit: break here, eat it */
                *line_len = i;
                *line_w = w;
                size_t used = i;
                while (used < len && s[used] == ' ')
                    used++;
                return used;
            }
            have_break = true;
            break_i = i;
            break_w = w;
            w += cw;
            i += n;
            continue;
        }

        if (w + cw > width)
        {
            if (w == 0)
            {
                /* glyph wider than the whole line: emit it alone */
                *line_len = n;
                *line_w = cw;
                return n;
            }
            if (have_break && break_i > 0)
            {
                *line_len = break_i;
                *line_w = break_w;
                size_t used = break_i;
                while (used < len && s[used] == ' ')
                    used++;
                return used;
            }
            /* single long word: hard break */
            *line_len = i;
            *line_w = w;
            return i;
        }
        w += cw;
        i += n;
    }
    *line_len = len;
    *line_w = w;
    return len;
}

/* iterate logical lines */
static const char *next_logical(const char *p, const char *end, size_t *len)
{
    const char *q = p;
    while (q < end && *q != '\n')
        q++;
    *len = (size_t)(q - p);
    return q < end ? q + 1 : end; /* start of the next line */
}

int tui__text_max_width(const tui_node *n)
{
    const char *p = n->text, *end = n->text + n->text_len;
    int best = 0;
    for (;;)
    {
        size_t len;
        const char *next = next_logical(p, end, &len);
        int w = tui__text_width(p, len);
        if (w > best)
            best = w;
        if (p + len >= end)
            break; /* last logical line (a trailing '\n' yields an empty one) */
        p = next;
    }
    return best;
}

int tui__text_min_width(const tui_node *n)
{
    if (n->a.text_wrap != TUI_TEXT_WRAP)
        return 0;
    int best = 0, cur = 0;
    size_t i = 0;
    while (i < n->text_len)
    {
        uint32_t cp;
        i += tui__utf8_decode(n->text + i, n->text_len - i, &cp);
        if (cp == ' ' || cp == '\n')
            cur = 0;
        else
        {
            cur += tui__cp_width(cp);
            if (cur > best)
                best = cur;
        }
    }
    return best;
}

/* Number of logical lines: a trailing '\n' starts one more (empty) line. */
static int logical_lines(const tui_node *n)
{
    int c = 1;
    for (size_t i = 0; i < n->text_len; i++)
        if (n->text[i] == '\n')
            c++;
    return c;
}

int tui__text_height_for_width(const tui_node *n, int w)
{
    if (n->a.text_wrap != TUI_TEXT_WRAP)
        return logical_lines(n);

    int lines = 0;
    const char *p = n->text, *end = n->text + n->text_len;
    for (;;)
    {
        size_t len;
        const char *next = next_logical(p, end, &len);
        if (len == 0)
            lines++;
        else
        {
            const char *q = p;
            size_t left = len;
            while (left > 0)
            {
                size_t ll;
                int lw;
                size_t used = wrap_next(q, left, w, &ll, &lw);
                if (used == 0)
                    used = 1; /* defensive: always progress */
                q += used;
                left -= used;
                lines++;
            }
        }
        if (p + len >= end)
            break;
        p = next;
    }
    return lines;
}

/* ------------------------------------------------------------------------ */
/* painting                                                                 */
/* ------------------------------------------------------------------------ */

static int align_offset(tui_text_align a, int avail, int w)
{
    if (w >= avail)
        return 0;
    if (a == TUI_TEXT_CENTER)
        return (avail - w) / 2;
    if (a == TUI_TEXT_RIGHT)
        return avail - w;
    return 0;
}

static void draw_line(tui_canvas *cv, const tui_node *n, int y, const char *s,
                      size_t len, int lw, tui_pen pen)
{
    int avail = tui_canvas_width(cv);
    if (n->a.text_wrap == TUI_TEXT_ELLIPSIS && lw > avail && avail > 0)
    {
        /* keep avail-1 columns, then an ellipsis */
        int budget = avail - 1, w = 0;
        size_t i = 0;
        while (i < len)
        {
            uint32_t cp;
            size_t k = tui__utf8_decode(s + i, len - i, &cp);
            int cw = tui__cp_width(cp);
            if (w + cw > budget)
                break;
            tui_canvas_put(cv, w, y, cp, pen);
            w += cw;
            i += k;
        }
        tui_canvas_put(cv, w, y, 0x2026, pen);
        return;
    }
    int x = align_offset(n->a.text_align, avail, lw);
    tui_canvas_textn(cv, x, y, s, len, pen);
}

void tui__text_paint(const tui_node *n, tui_canvas *cv, tui_pen pen)
{
    int avail = tui_canvas_width(cv), rows = tui_canvas_height(cv);
    const char *p = n->text, *end = n->text + n->text_len;
    int y = 0;
    for (;;)
    {
        size_t len;
        const char *next = next_logical(p, end, &len);

        if (n->a.text_wrap == TUI_TEXT_WRAP)
        {
            const char *q = p;
            size_t left = len;
            if (left == 0)
                y++;
            while (left > 0 && y < rows)
            {
                size_t ll;
                int lw;
                size_t used = wrap_next(q, left, avail, &ll, &lw);
                if (used == 0)
                    used = 1;
                draw_line(cv, n, y, q, ll, lw, pen);
                q += used;
                left -= used;
                y++;
            }
        }
        else
        {
            if (y < rows)
                draw_line(cv, n, y, p, len, tui__text_width(p, len), pen);
            y++;
        }
        if (y >= rows || p + len >= end)
            break;
        p = next;
    }
}

/* ------------------------------------------------------------------------ */
/* public wrappers                                                          */
/* ------------------------------------------------------------------------ */

size_t tui_utf8_decode(const char *s, size_t len, uint32_t *cp)
{
    return tui__utf8_decode(s, len, cp);
}

size_t tui_utf8_encode(uint32_t cp, char out[4])
{
    if (cp < 0x80)
    {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800)
    {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000)
    {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

int tui_cp_width(uint32_t cp)
{
    return tui__cp_width(cp);
}

int tui_text_width(const char *utf8, size_t len)
{
    return tui__text_width(utf8, len);
}
