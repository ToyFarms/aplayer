#include "ui_ansi.h"

#include "tui.h"

#include <stdbool.h>

static color_t xterm256(int n)
{
    static const int basic[16][3] = {
        {0,   0,   0  },
        {205, 0,   0  },
        {0,   205, 0  },
        {205, 205, 0  },
        {0,   0,   238},
        {205, 0,   205},
        {0,   205, 205},
        {229, 229, 229},
        {127, 127, 127},
        {255, 0,   0  },
        {0,   255, 0  },
        {255, 255, 0  },
        {92,  92,  255},
        {255, 0,   255},
        {0,   255, 255},
        {255, 255, 255}
    };
    if (n < 16)
        return COLOR(basic[n][0], basic[n][1], basic[n][2]);
    if (n < 232)
    {
        n -= 16;
        int r = n / 36, g = (n / 6) % 6, b = n % 6;
        return COLOR(r ? 55 + r * 40 : 0, g ? 55 + g * 40 : 0,
                     b ? 55 + b * 40 : 0);
    }
    int v = 8 + (n - 232) * 10;
    return COLOR(v, v, v);
}

static void apply_sgr(const int *p, int np, color_t *fg, color_t *bg, bool *rev)
{
    if (np == 0)
    {
        *fg = *bg = COLOR_NONE;
        *rev = false;
        return;
    }
    for (int i = 0; i < np; i++)
    {
        int c = p[i];
        if (c == 0)
        {
            *fg = *bg = COLOR_NONE;
            *rev = false;
        }
        else if (c == 7)
            *rev = true;
        else if (c == 27)
            *rev = false;
        else if (c == 39)
            *fg = COLOR_NONE;
        else if (c == 49)
            *bg = COLOR_NONE;
        else if ((c == 38 || c == 48) && i + 1 < np)
        {
            color_t col = COLOR_NONE;
            if (p[i + 1] == 2 && i + 4 < np)
            {
                col = COLOR(p[i + 2] & 255, p[i + 3] & 255, p[i + 4] & 255);
                i += 4;
            }
            else if (p[i + 1] == 5 && i + 2 < np)
            {
                col = xterm256(p[i + 2] & 255);
                i += 2;
            }
            if (c == 38)
                *fg = col;
            else
                *bg = col;
        }
    }
}

void ui_ansi_decode(const char *s, size_t len, ui_cell *grid, int w, int h)
{
    int x = 0, y = 0;
    color_t fg = COLOR_NONE, bg = COLOR_NONE;
    bool rev = false;
    size_t i = 0;

    while (i < len)
    {
        unsigned char ch = (unsigned char)s[i];
        if (ch == 0x1b && i + 1 < len && s[i + 1] == '[')
        {
            i += 2;
            if (i < len && s[i] >= '<' && s[i] <= '?') /* private marker */
                i++;
            int params[32], np = 0, cur = 0;
            bool have = false;
            while (i < len && (s[i] == ';' || (s[i] >= '0' && s[i] <= '9')))
            {
                if (s[i] == ';')
                {
                    if (np < 32)
                        params[np++] = cur;
                    cur = 0;
                    have = false;
                }
                else
                {
                    cur = cur * 10 + (s[i] - '0');
                    have = true;
                }
                i++;
            }
            if (have || np > 0)
                if (np < 32)
                    params[np++] = cur;
            char fin = i < len ? s[i++] : 0;
            if (fin == 'H')
            {
                y = (np > 0 ? params[0] : 1) - 1;
                x = (np > 1 ? params[1] : 1) - 1;
            }
            else if (fin == 'm')
                apply_sgr(params, np, &fg, &bg, &rev);
            else if (fin == 'C')
                x += np > 0 ? params[0] : 1;
            else if (fin == 'D')
                x -= np > 0 ? params[0] : 1;
            else if (fin == 'B')
                y += np > 0 ? params[0] : 1;
            else if (fin == 'A')
                y -= np > 0 ? params[0] : 1;
            continue;
        }
        if (ch == '\n')
        {
            y++;
            x = 0;
            i++;
            continue;
        }
        uint32_t cp;
        size_t k = tui_utf8_decode(s + i, len - i, &cp);
        i += k ? k : 1;
        if (cp < 0x20)
            continue;
        int cw = tui_cp_width(cp);
        if (x >= 0 && x < w && y >= 0 && y < h && cw > 0)
            grid[y * w + x] = (ui_cell){cp, fg, bg, rev};
        x += cw > 0 ? cw : 1;
    }
}
