#include "tui_internal.h"

/*
 * Intrinsic sizes.  Everything here is border-box and cached on the node;
 * tui__mark_layout() invalidates the cache of the dirty node and of every
 * ancestor up to the nearest fixed-size boundary, which is exactly the set of
 * nodes whose intrinsic size can have changed.
 */

#define TUI_INF (1 << 28)

/* cells -> value, percent -> value if `base` is definite, else -1 */
int tui__resolve(tui_dim d, int base)
{
    if (d.unit == TUI_UNIT_CELLS)
        return d.v < 0 ? 0 : (int)d.v;
    if (d.unit == TUI_UNIT_PERCENT && base >= 0)
        return (int)(d.v * (float)base / 100.0f);
    return -1;
}

static int clamp_dim(int v, tui_dim mn, tui_dim mx)
{
    /* percent bounds are unresolvable without a parent: ignored here */
    int lo = mn.unit == TUI_UNIT_CELLS ? tui__resolve(mn, -1) : 0;
    int hi = mx.unit == TUI_UNIT_CELLS ? tui__resolve(mx, -1) : TUI_INF;
    if (v > hi)
        v = hi;
    if (v < lo)
        v = lo;
    return v;
}

static bool is_row(const tui_node *n)
{
    return n->a.dir == TUI_ROW || n->a.dir == TUI_ROW_REVERSE;
}

int tui__gap_main(const tui_node *n)
{
    int g = n->a.gap;
    if (is_row(n))
        return n->a.col_gap ? n->a.col_gap : g;
    return n->a.row_gap ? n->a.row_gap : g;
}

int tui__gap_cross(const tui_node *n)
{
    int g = n->a.gap;
    if (is_row(n))
        return n->a.row_gap ? n->a.row_gap : g;
    return n->a.col_gap ? n->a.col_gap : g;
}

int tui__max_content_w(tui_node *n)
{
    if (n->flags & NF_MC_VALID)
        return n->mcw;

    tui_edges in = tui__insets(n);
    int ih = in.left + in.right;
    int w;

    if (n->a.width.unit == TUI_UNIT_CELLS)
    {
        w = tui__resolve(n->a.width, -1);
    }
    else if (n->kind == TUI_KIND_TEXT)
    {
        w = tui__text_max_width(n) + ih;
    }
    else if (n->first == NULL)
    {
        w = ih;
        if (n->a.measure)
            w += tui__max(0, n->a.measure(n, -1, n->a.userdata).w);
    }
    else
    {
        bool row = is_row(n);
        int sum = 0, widest = 0, cnt = 0;
        for (tui_node *c = n->first; c; c = c->next)
        {
            if (c->a.display == TUI_DISPLAY_NONE ||
                c->a.position == TUI_POS_ABSOLUTE)
                continue;
            int cw;
            if (row && c->a.basis.unit == TUI_UNIT_CELLS)
                cw = tui__resolve(c->a.basis, -1);
            else
                cw = tui__max_content_w(c);
            cw += c->a.margin.left + c->a.margin.right;
            sum += cw;
            widest = tui__max(widest, cw);
            cnt++;
        }
        w = ih + (row ? sum + (cnt > 1 ? (cnt - 1) * tui__gap_main(n) : 0)
                      : widest);
    }

    w = clamp_dim(w, n->a.min_w, n->a.max_w);
    w = tui__max(w, ih);
    n->mcw = w;
    n->flags |= NF_MC_VALID;
    return w;
}

int tui__height_for_width(tui_node *n, int w)
{
    tui_edges in = tui__insets(n);
    int ih = in.left + in.right, iv = in.top + in.bottom;

    if (n->a.height.unit == TUI_UNIT_CELLS)
        return tui__max(iv, clamp_dim(tui__resolve(n->a.height, -1),
                                      n->a.min_h, n->a.max_h));

    for (int i = 0; i < n->hfw_n; i++)
        if (n->hfw_w[i] == w)
            return n->hfw_h[i];

    int h;
    int cw = tui__max(0, w - ih);
    if (n->kind == TUI_KIND_TEXT)
    {
        h = tui__text_height_for_width(n, cw) + iv;
    }
    else if (n->first == NULL)
    {
        h = iv;
        if (n->a.measure)
            h += tui__max(0, n->a.measure(n, cw, n->a.userdata).h);
    }
    else
    {
        h = tui__flex_height(n, w);
    }

    h = clamp_dim(h, n->a.min_h, n->a.max_h);
    h = tui__max(h, iv);

    int slot;
    if (n->hfw_n < TUI_HFW_CACHE)
        slot = n->hfw_n++;
    else
    {
        slot = n->hfw_next;
        n->hfw_next = (uint8_t)((n->hfw_next + 1) % TUI_HFW_CACHE);
    }
    n->hfw_w[slot] = w;
    n->hfw_h[slot] = h;
    return h;
}
