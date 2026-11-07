#ifndef __TUI_VT_H
#define __TUI_VT_H

/* Test helper: a tiny terminal emulator that understands exactly the escape
 * sequences tui emits (CUP, CUF, CUB, ED 2, SGR, DEC private modes ignored),
 * plus helpers to compare it against the renderer's own buffers. */

#include "tui.h"
#include "tui_internal.h"
#include "wcwidth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct vt_cell
{
    uint32_t cp, fg, bg;
    uint16_t attrs, w;
} vt_cell;

typedef struct vt
{
    int W, H;
    vt_cell *cells;
    int cx, cy;
    uint32_t fg, bg;
    uint16_t attrs;
    size_t total_bytes;
    bool cursor_vis; /* DECTCEM */
    int errors; /* printed outside the screen, bad sequence, ... */
    char why[96];
} vt;

static vt_cell vt_blank(void)
{
    return (vt_cell){' ', 0, 0, 0, 1};
}

static vt vt_new(int W, int H)
{
    vt t = {0};
    t.W = W;
    t.H = H;
    t.cells = malloc((size_t)W * H * sizeof(vt_cell));
    for (int i = 0; i < W * H; i++)
        t.cells[i] = vt_blank();
    return t;
}

static void vt_err(vt *t, const char *why, int a, int b)
{
    if (t->errors++ == 0)
        snprintf(t->why, sizeof(t->why), "%s (%d,%d) cursor=(%d,%d)", why, a, b, t->cx, t->cy);
}

static void vt_free(vt *t)
{
    free(t->cells);
}

static void vt_resize(vt *t, int W, int H)
{
    free(t->cells);
    *t = vt_new(W, H);
}

static void vt_sgr(vt *t, const int *p, int n)
{
    if (n == 0)
    {
        t->fg = t->bg = 0;
        t->attrs = 0;
        return;
    }
    for (int i = 0; i < n; i++)
    {
        int c = p[i];
        switch (c)
        {
        case 0:
            t->fg = t->bg = 0;
            t->attrs = 0;
            break;
        case 1: t->attrs |= TUI_BOLD; break;
        case 2: t->attrs |= TUI_DIM; break;
        case 3: t->attrs |= TUI_ITALIC; break;
        case 4: t->attrs |= TUI_UNDERLINE; break;
        case 7: t->attrs |= TUI_REVERSE; break;
        case 9: t->attrs |= TUI_STRIKE; break;
        case 39: t->fg = 0; break;
        case 49: t->bg = 0; break;
        case 38:
        case 48:
            if (i + 4 < n + 0 && p[i + 1] == 2)
            {
                uint32_t v = 0xFF000000u | ((uint32_t)p[i + 2] << 16) |
                             ((uint32_t)p[i + 3] << 8) | (uint32_t)p[i + 4];
                if (c == 38)
                    t->fg = v;
                else
                    t->bg = v;
                i += 4;
            }
            else if (i + 2 < n + 0 && p[i + 1] == 5)
            {
                /* 256-colour: store the index in the low bits with alpha */
                uint32_t v = 0xFE000000u | (uint32_t)p[i + 2];
                if (c == 38)
                    t->fg = v;
                else
                    t->bg = v;
                i += 2;
            }
            else
                vt_err(t, "bad extended colour", c, n);
            break;
        default:
            vt_err(t, "unknown SGR", c, 0);
        }
    }
}

static void vt_feed(vt *t, const char *s, size_t len)
{
    t->total_bytes += len;
    size_t i = 0;
    while (i < len)
    {
        unsigned char ch = (unsigned char)s[i];
        if (ch == 0x1b)
        {
            if (i + 1 >= len || s[i + 1] != '[')
            {
                vt_err(t, "bare ESC", 0, 0);
                i++;
                continue;
            }
            i += 2;
            bool priv = false;
            if (i < len && s[i] == '?')
            {
                priv = true;
                i++;
            }
            int params[32], np = 0;
            int cur = -1;
            while (i < len && ((s[i] >= '0' && s[i] <= '9') || s[i] == ';'))
            {
                if (s[i] == ';')
                {
                    if (np < 32)
                        params[np++] = cur < 0 ? 0 : cur;
                    cur = -1;
                }
                else
                    cur = (cur < 0 ? 0 : cur) * 10 + (s[i] - '0');
                i++;
            }
            if (cur >= 0 || (np > 0 && s[i - 1] == ';'))
            {
                if (np < 32)
                    params[np++] = cur < 0 ? 0 : cur;
            }
            if (i >= len)
            {
                vt_err(t, "truncated CSI", 0, 0);
                break;
            }
            char f = s[i++];
            if (priv)
            {
                if (np > 0 && params[0] == 25)
                    t->cursor_vis = (f == 'h');
                continue;
            }
            if (f == ' ' && i < len && s[i] == 'q') /* DECSCUSR: cursor shape */
            {
                i++;
                continue;
            }
            int p0 = np > 0 ? params[0] : 0;
            switch (f)
            {
            case 'H':
                t->cy = (np > 0 ? params[0] : 1) - 1;
                t->cx = (np > 1 ? params[1] : 1) - 1;
                if (t->cy < 0 || t->cy >= t->H || t->cx < 0 || t->cx >= t->W)
                    vt_err(t, "CUP out of range", params[0], np > 1 ? params[1] : 1);
                break;
            case 'C':
                t->cx += p0 ? p0 : 1;
                break;
            case 'D':
                t->cx -= p0 ? p0 : 1;
                break;
            case 'J':
                for (int k = 0; k < t->W * t->H; k++)
                    t->cells[k] = vt_blank();
                break;
            case 'm':
                vt_sgr(t, params, np);
                break;
            default:
                vt_err(t, "unknown CSI final", f, 0);
            }
            continue;
        }

        uint32_t cp;
        size_t n = tui__utf8_decode(s + i, len - i, &cp);
        i += n;
        int w = tui__cp_width(cp);
        if (w == 0)
            continue;
        if (t->cy < 0 || t->cy >= t->H || t->cx < 0 || t->cx + w > t->W)
        {
            vt_err(t, "print off-screen", (int)cp, w); /* never allowed */
            continue;
        }
        vt_cell *row = t->cells + (size_t)t->cy * t->W;
        for (int k = 0; k < w; k++)
        {
            vt_cell *c = &row[t->cx + k];
            /* overwriting half a wide glyph blanks the other half */
            if (c->w == 2 && t->cx + k + 1 < t->W)
                row[t->cx + k + 1] = vt_blank();
            if (c->w == 0 && t->cx + k > 0)
                row[t->cx + k - 1] = vt_blank();
            if (k == 0)
                *c = (vt_cell){cp, t->fg, t->bg, t->attrs, (uint16_t)w};
            else
                *c = (vt_cell){0, t->fg, t->bg, t->attrs, 0};
        }
        t->cx += w;
    }
}

/* write callback for tui_options */
static void vt_write_cb(const char *d, size_t n, void *ud)
{
    vt_feed((vt *)ud, d, n);
}

/* what the emulator should have recorded for a buffer colour */
static uint32_t vt_expect_color(uint32_t packed, bool mode256)
{
    if (!mode256 || packed == 0)
        return packed;
    color_t c = COLOR((packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF);
    return 0xFE000000u | (uint32_t)color_term_pallete(c);
}

/* returns the index of the first differing cell, or -1 */
static int vt_diff_buf(const vt *t, const tui_cell *buf, bool mode256)
{
    for (int i = 0; i < t->W * t->H; i++)
    {
        const vt_cell *a = &t->cells[i];
        const tui_cell *b = &buf[i];
        if (a->cp != b->cp || a->fg != vt_expect_color(b->fg, mode256) ||
            a->bg != vt_expect_color(b->bg, mode256) || a->attrs != b->attrs ||
            a->w != b->w)
            return i;
    }
    return -1;
}

/* ---- text dump helpers --------------------------------------------------- */

static void vt_row_text(const vt *t, int y, char *out, size_t cap)
{
    size_t o = 0;
    for (int x = 0; x < t->W; x++)
    {
        const vt_cell *c = &t->cells[(size_t)y * t->W + x];
        if (c->w == 0)
            continue;
        uint32_t cp = c->cp;
        char tmp[4];
        int n;
        if (cp < 0x80) { tmp[0] = (char)cp; n = 1; }
        else if (cp < 0x800) { tmp[0] = (char)(0xC0 | (cp >> 6)); tmp[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
        else if (cp < 0x10000) { tmp[0] = (char)(0xE0 | (cp >> 12)); tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
        else { tmp[0] = (char)(0xF0 | (cp >> 18)); tmp[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); tmp[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
        if (o + n + 1 < cap)
        {
            memcpy(out + o, tmp, (size_t)n);
            o += (size_t)n;
        }
    }
    /* trim trailing blanks */
    while (o > 0 && out[o - 1] == ' ')
        o--;
    out[o] = '\0';
}

static void vt_dump(const vt *t)
{
    char line[1024];
    for (int y = 0; y < t->H; y++)
    {
        vt_row_text(t, y, line, sizeof(line));
        fprintf(stderr, "|%s\n", line);
    }
}

#endif /* __TUI_VT_H */
