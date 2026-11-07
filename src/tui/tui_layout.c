#include "tui_internal.h"

#include <math.h>

#define TUI_INF   (1 << 28)
#define STACK_MAX 40

/*
 * Flexbox layout in integer cells (box-sizing: border-box everywhere).
 *
 * Supported: direction (+reverse), nowrap/wrap, justify-content, align-items /
 * align-self, align-content, flex-grow/shrink/basis, width/height/min/max in
 * cells or percent, margin, padding, border, gap, display:none and
 * position:absolute with left/top/right/bottom.
 *
 * A node's rect is stored relative to its parent's border box.  Layout only
 * descends into children that are dirty or whose assigned size changed;
 * everything that changed position/size is reported to the damage list.
 */

typedef struct fitem
{
    tui_node *n;
    int ms, me;   /* physical margins along the main axis (left/top, right/bottom) */
    int cs, ce;   /* physical margins along the cross axis */
    int basis, hyp, main;
    int min_m, max_m;
    int cross, cmin, cmax;
    bool cross_auto;
    double grow, shrink, target, clamped;
    bool frozen;
    tui_align align;
    int pos_m, pos_c; /* border-box offset inside the inner box (physical) */
} fitem;

typedef struct fline
{
    int first, count;
    int cross;
    int pos;
} fline;

static void layout_node(tui_node *n, bool vis, int ox, int oy);

static int round_i(double v)
{
    return (int)floor(v + 0.5);
}

static bool row_dir(const tui_node *n)
{
    return n->a.dir == TUI_ROW || n->a.dir == TUI_ROW_REVERSE;
}

static bool rev_dir(const tui_node *n)
{
    return n->a.dir == TUI_ROW_REVERSE || n->a.dir == TUI_COLUMN_REVERSE;
}

static tui_align item_align(const tui_node *container, const tui_node *c)
{
    tui_align a = c->a.align_self != TUI_ALIGN_AUTO ? c->a.align_self
                                                    : container->a.align_items;
    return a == TUI_ALIGN_AUTO ? TUI_ALIGN_STRETCH : a;
}

static int clamp_cells(int v, tui_dim mn, tui_dim mx)
{
    int lo = mn.unit == TUI_UNIT_CELLS ? tui__resolve(mn, -1) : 0;
    int hi = mx.unit == TUI_UNIT_CELLS ? tui__resolve(mx, -1) : TUI_INF;
    return v > hi ? hi : (v < lo ? lo : v);
}

/* ---- distribution of free space (justify-content / align-content) ------- */

enum dist
{
    DIST_START,
    DIST_END,
    DIST_CENTER,
    DIST_BETWEEN,
    DIST_AROUND,
    DIST_EVENLY,
};

/* extra space that goes before item i of n, given `free` >= 0 to share */
static int dist_before(enum dist d, int free, int n, int i)
{
    switch (d)
    {
    case DIST_END:
        return free;
    case DIST_CENTER:
        return free / 2;
    case DIST_BETWEEN:
        return n > 1 ? round_i((double)free * i / (n - 1)) : 0;
    case DIST_AROUND:
        return round_i((double)free * (2 * i + 1) / (2.0 * n));
    case DIST_EVENLY:
        return round_i((double)free * (i + 1) / (n + 1.0));
    default:
        return 0;
    }
}

static enum dist justify_dist(tui_justify j)
{
    switch (j)
    {
    case TUI_JUSTIFY_END:
        return DIST_END;
    case TUI_JUSTIFY_CENTER:
        return DIST_CENTER;
    case TUI_JUSTIFY_BETWEEN:
        return DIST_BETWEEN;
    case TUI_JUSTIFY_AROUND:
        return DIST_AROUND;
    case TUI_JUSTIFY_EVENLY:
        return DIST_EVENLY;
    default:
        return DIST_START;
    }
}

/* ---- flex item preparation --------------------------------------------- */

static void fill_item(const tui_node *n, tui_node *c, fitem *it, bool row,
                      int avail_m, int avail_c, int iw)
{
    tui_edges mg = c->a.margin, ci = tui__insets(c);
    memset(it, 0, sizeof(*it));
    it->n = c;
    it->ms = row ? mg.left : mg.top;
    it->me = row ? mg.right : mg.bottom;
    it->cs = row ? mg.top : mg.left;
    it->ce = row ? mg.bottom : mg.right;
    int ins_m = row ? ci.left + ci.right : ci.top + ci.bottom;
    int ins_c = row ? ci.top + ci.bottom : ci.left + ci.right;

    tui_dim dm = row ? c->a.width : c->a.height;
    tui_dim dc = row ? c->a.height : c->a.width;
    tui_dim mnm = row ? c->a.min_w : c->a.min_h;
    tui_dim mxm = row ? c->a.max_w : c->a.max_h;
    tui_dim mnc = row ? c->a.min_h : c->a.min_w;
    tui_dim mxc = row ? c->a.max_h : c->a.max_w;

    it->grow = c->a.grow > 0 ? c->a.grow : 0;
    it->shrink =
        c->a.shrink == 0 ? 1.0 : (c->a.shrink < 0 ? 0.0 : (double)c->a.shrink);
    it->align = item_align(n, c);
    if (n->a.scroll)
        it->shrink = 0; /* scroll containers let their content overflow */

    int v = tui__resolve(mnm, avail_m);
    it->min_m = tui__max(ins_m, v < 0 ? 0 : v);
    v = tui__resolve(mxm, avail_m);
    it->max_m = v < 0 ? TUI_INF : tui__max(v, it->min_m);
    v = tui__resolve(mnc, avail_c);
    it->cmin = tui__max(ins_c, v < 0 ? 0 : v);
    v = tui__resolve(mxc, avail_c);
    it->cmax = v < 0 ? TUI_INF : tui__max(v, it->cmin);

    bool content_leaf =
        c->kind == TUI_KIND_TEXT || (c->first == NULL && c->a.measure);
    bool auto_main = dm.unit == TUI_UNIT_AUTO &&
                     c->a.basis.unit == TUI_UNIT_AUTO &&
                     mnm.unit == TUI_UNIT_AUTO;

    /* CSS min-height:auto for definite sizes: min(specified, content), so a
     * 1-row header is never squeezed to 0 by a tall sibling */
    bool def_main = dm.unit == TUI_UNIT_CELLS && !c->a.scroll &&
                    c->a.basis.unit == TUI_UNIT_AUTO &&
                    mnm.unit == TUI_UNIT_AUTO;

    if (row)
    {
        int b = tui__resolve(c->a.basis, avail_m);
        if (b < 0)
            b = tui__resolve(dm, avail_m);
        if (b < 0)
            b = tui__max_content_w(c);
        it->basis = tui__max(0, b);
        /* wrapping text keeps its longest word */
        if (auto_main && c->kind == TUI_KIND_TEXT &&
            c->a.text_wrap == TUI_TEXT_WRAP)
            it->min_m = tui__max(it->min_m, tui__text_min_width(c) + ins_m);
        if (def_main)
            it->min_m = tui__max(it->min_m,
                                 tui__min(tui__resolve(dm, avail_m),
                                          tui__max_content_w(c)));
    }
    else
    {
        /* the cross axis is the width: needed to know the height */
        int cw = tui__resolve(dc, iw);
        it->cross_auto = cw < 0;
        if (cw < 0)
        {
            if (it->align == TUI_ALIGN_STRETCH)
                cw = iw - it->cs - it->ce;
            else
                cw = tui__min(tui__max_content_w(c), iw - it->cs - it->ce);
        }
        cw = tui__clamp(cw, 0, TUI_INF);
        cw = cw > it->cmax ? it->cmax : (cw < it->cmin ? it->cmin : cw);
        it->cross = cw;

        int b = tui__resolve(c->a.basis, avail_m);
        if (b < 0)
            b = tui__resolve(dm, avail_m);
        if (b < 0)
            b = tui__height_for_width(c, cw);
        it->basis = tui__max(0, b);
        /* text/leaf content never shrinks below its own height */
        if (auto_main && content_leaf)
            it->min_m = tui__max(it->min_m, tui__height_for_width(c, cw));
        if (def_main)
            it->min_m = tui__max(it->min_m,
                                 tui__min(tui__resolve(dm, avail_m),
                                          tui__height_for_width(c, cw)));
    }

    it->hyp = it->basis > it->max_m ? it->max_m
                                    : (it->basis < it->min_m ? it->min_m
                                                             : it->basis);
}

/* CSS "resolve flexible lengths", with exact integer results */
static void resolve_line(fitem *it, int count, int avail_m, int gap_m)
{
    double gaps = (double)gap_m * (count > 1 ? count - 1 : 0);
    double hyp_sum = gaps;
    for (int i = 0; i < count; i++)
    {
        it[i].frozen = false;
        it[i].main = it[i].hyp;
        hyp_sum += it[i].hyp + it[i].ms + it[i].me;
    }
    if (avail_m < 0)
        return;

    bool growing = hyp_sum <= avail_m;
    for (int i = 0; i < count; i++)
    {
        double f = growing ? it[i].grow : it[i].shrink;
        if (f == 0 || (growing ? it[i].basis > it[i].hyp
                               : it[i].basis < it[i].hyp))
        {
            it[i].frozen = true;
            it[i].main = it[i].hyp;
        }
    }

    for (int iter = 0; iter <= count + 1; iter++)
    {
        double used = gaps, factor_sum = 0;
        int nun = 0;
        for (int i = 0; i < count; i++)
        {
            if (it[i].frozen)
                used += it[i].main + it[i].ms + it[i].me;
            else
            {
                used += it[i].basis + it[i].ms + it[i].me;
                nun++;
                factor_sum += growing ? it[i].grow : it[i].shrink * it[i].basis;
            }
        }
        if (nun == 0)
            return;

        double rem = avail_m - used;
        if (growing && factor_sum < 1.0)
            rem *= factor_sum;

        double viol = 0;
        for (int i = 0; i < count; i++)
        {
            if (it[i].frozen)
                continue;
            double t = it[i].basis;
            if (factor_sum > 0)
                t += rem * (growing ? it[i].grow
                                    : it[i].shrink * it[i].basis) /
                     factor_sum;
            it[i].target = t;
            double c = t < it[i].min_m ? it[i].min_m
                                       : (t > it[i].max_m ? it[i].max_m : t);
            it[i].clamped = c;
            viol += c - t;
        }

        if (viol == 0)
        {
            /* cumulative rounding keeps the exact integer total */
            double acc = 0;
            int prev = 0;
            for (int i = 0; i < count; i++)
            {
                if (it[i].frozen)
                    continue;
                acc += it[i].target;
                int r = round_i(acc);
                it[i].main = r - prev;
                prev = r;
                it[i].frozen = true;
            }
            return;
        }
        for (int i = 0; i < count; i++)
        {
            if (it[i].frozen)
                continue;
            bool min_v = it[i].clamped > it[i].target;
            bool max_v = it[i].clamped < it[i].target;
            if ((viol > 0 && min_v) || (viol < 0 && max_v))
            {
                it[i].main = (int)it[i].clamped;
                it[i].frozen = true;
            }
        }
    }
    for (int i = 0; i < count; i++) /* unreachable safety net */
        if (!it[i].frozen)
            it[i].main = it[i].hyp;
}

/* ---- committing a child's rect ------------------------------------------ */

static void commit_child(tui_node *n, tui_node *c, tui_rect nr, bool vis,
                         int ox, int oy)
{
    bool was = (c->flags & NF_LAID_OUT) != 0;
    tui_rect old = c->rect;
    bool changed = !was || old.x != nr.x || old.y != nr.y || old.w != nr.w ||
                   old.h != nr.h;
    bool size_changed = !was || old.w != nr.w || old.h != nr.h;
    int cx = ox + nr.x - n->scroll_x, cy = oy + nr.y - n->scroll_y;

    if (vis && changed)
    {
        if (was)
            tui__damage_add(n->ctx, (tui_rect){ox + old.x - n->scroll_x,
                                               oy + old.y - n->scroll_y, old.w,
                                               old.h});
        tui__damage_add(n->ctx, (tui_rect){cx, cy, nr.w, nr.h});
    }
    c->rect = nr;
    c->flags |= NF_LAID_OUT;
    if (size_changed || (c->flags & NF_LAYOUT_DIRTY))
        layout_node(c, vis, cx, cy);
}

/* ---- absolute children --------------------------------------------------- */

static tui_rect abs_child_rect(const tui_node *n, tui_node *c, int W, int H)
{
    int b = tui__border_w(n);
    int pw = tui__max(0, W - 2 * b), ph = tui__max(0, H - 2 * b);
    tui_edges mg = c->a.margin, ci = tui__insets(c);

    int l = tui__resolve(c->a.left, pw), r = tui__resolve(c->a.right, pw);
    int t = tui__resolve(c->a.top, ph), bo = tui__resolve(c->a.bottom, ph);

    int w = tui__resolve(c->a.width, pw);
    if (w < 0)
    {
        if (l >= 0 && r >= 0)
            w = pw - l - r - mg.left - mg.right;
        else
        {
            int room = pw - (l >= 0 ? l : 0) - (r >= 0 ? r : 0) - mg.left -
                       mg.right;
            w = tui__min(tui__max_content_w(c), tui__max(0, room));
        }
    }
    w = clamp_cells(tui__max(w, ci.left + ci.right), c->a.min_w, c->a.max_w);

    int h = tui__resolve(c->a.height, ph);
    if (h < 0)
    {
        if (t >= 0 && bo >= 0)
            h = ph - t - bo - mg.top - mg.bottom;
        else
            h = tui__height_for_width(c, w);
    }
    h = clamp_cells(tui__max(h, ci.top + ci.bottom), c->a.min_h, c->a.max_h);

    int x = l >= 0 ? l + mg.left : (r >= 0 ? pw - r - mg.right - w : mg.left);
    int y = t >= 0 ? t + mg.top : (bo >= 0 ? ph - bo - mg.bottom - h : mg.top);
    return (tui_rect){b + x, b + y, w, h};
}

/* ---- the algorithm ------------------------------------------------------- */

/* Lay out the children of `n` given its border-box size W x H (H < 0: auto).
 * Returns the (possibly computed) border-box height.  With commit == false
 * nothing but caches is touched. */
static int flex_layout(tui_node *n, int W, int H, bool commit, bool vis,
                       int ox, int oy)
{
    tui_edges in = tui__insets(n);
    int iw = tui__max(0, W - in.left - in.right);
    int ih = H >= 0 ? tui__max(0, H - in.top - in.bottom) : -1;
    bool row = row_dir(n), rev = rev_dir(n);
    int gap_m = tui__gap_main(n), gap_c = tui__gap_cross(n);
    int avail_m = row ? iw : ih;
    int avail_c = row ? ih : iw;

    int cnt = 0;
    for (tui_node *c = n->first; c; c = c->next)
        if (c->a.display != TUI_DISPLAY_NONE &&
            c->a.position != TUI_POS_ABSOLUTE)
            cnt++;

    fitem item_stack[STACK_MAX];
    fline line_stack[STACK_MAX];
    fitem *items = cnt <= STACK_MAX ? item_stack : malloc(cnt * sizeof(fitem));
    fline *lines = cnt <= STACK_MAX ? line_stack : malloc(cnt * sizeof(fline));
    if (items == NULL || lines == NULL)
        abort();

    /* 1. items + hypothetical sizes */
    int k = 0;
    for (tui_node *c = n->first; c; c = c->next)
    {
        if (c->a.display == TUI_DISPLAY_NONE)
        {
            if (commit)
                c->flags &= ~NF_LAID_OUT;
            continue;
        }
        if (c->a.position == TUI_POS_ABSOLUTE)
            continue;
        fill_item(n, c, &items[k++], row, avail_m, avail_c, iw);
    }

    /* 2. split into lines */
    int nlines = 0;
    if (cnt > 0)
    {
        nlines = 1;
        lines[0] = (fline){0, 0, 0, 0};
        int sum = 0;
        for (int i = 0; i < cnt; i++)
        {
            int outer = items[i].hyp + items[i].ms + items[i].me;
            fline *L = &lines[nlines - 1];
            if (n->a.wrap == TUI_WRAP && avail_m >= 0 && L->count > 0 &&
                sum + gap_m + outer > avail_m)
            {
                lines[nlines] = (fline){i, 0, 0, 0};
                L = &lines[nlines++];
                sum = 0;
            }
            sum += (L->count ? gap_m : 0) + outer;
            L->count++;
        }
    }

    /* 3. flexible lengths per line */
    for (int l = 0; l < nlines; l++)
        resolve_line(&items[lines[l].first], lines[l].count, avail_m, gap_m);

    /* 4. cross sizes (natural) and line cross sizes */
    for (int l = 0; l < nlines; l++)
    {
        fline *L = &lines[l];
        int line_cross = 0;
        for (int i = L->first; i < L->first + L->count; i++)
        {
            fitem *it = &items[i];
            if (row)
            {
                int hc = tui__resolve(it->n->a.height, ih);
                it->cross_auto = hc < 0;
                if (hc < 0)
                    hc = tui__height_for_width(it->n, it->main);
                it->cross = hc > it->cmax ? it->cmax
                                          : (hc < it->cmin ? it->cmin : hc);
            }
            line_cross = tui__max(line_cross, it->cross + it->cs + it->ce);
        }
        L->cross = line_cross;
    }

    /* 5. auto container height */
    if (H < 0)
    {
        int total = 0;
        if (row)
        {
            for (int l = 0; l < nlines; l++)
                total += lines[l].cross + (l ? gap_c : 0);
        }
        else if (nlines > 0)
        {
            /* a column with unknown height never wraps: one line */
            fline *L = &lines[0];
            for (int i = L->first; i < L->first + L->count; i++)
                total += items[i].main + items[i].ms + items[i].me +
                         (i > L->first ? gap_m : 0);
        }
        H = clamp_cells(total + in.top + in.bottom, n->a.min_h, n->a.max_h);
        H = tui__max(H, in.top + in.bottom);
        ih = tui__max(0, H - in.top - in.bottom);
        avail_m = row ? iw : ih;
        avail_c = row ? ih : iw;
    }

    if (!commit)
    {
        if (items != item_stack)
            free(items);
        if (lines != line_stack)
            free(lines);
        return H;
    }

    /* 6. a single-line container fills the cross axis */
    if (nlines == 1 && n->a.wrap == TUI_NOWRAP)
        lines[0].cross = avail_c;

    /* 7. align-content: place (and maybe grow) the lines */
    {
        int used = gap_c * (nlines > 1 ? nlines - 1 : 0);
        for (int l = 0; l < nlines; l++)
            used += lines[l].cross;
        int leftover = avail_c - used;
        enum dist d = DIST_START;
        bool stretch = false;
        switch (n->a.align_content)
        {
        case TUI_CONTENT_STRETCH:
            stretch = true;
            break;
        case TUI_CONTENT_END:
            d = DIST_END;
            break;
        case TUI_CONTENT_CENTER:
            d = DIST_CENTER;
            break;
        case TUI_CONTENT_BETWEEN:
            d = DIST_BETWEEN;
            break;
        case TUI_CONTENT_AROUND:
            d = DIST_AROUND;
            break;
        default:
            break;
        }
        if (leftover < 0)
        {
            d = DIST_START;
            stretch = false;
            leftover = 0;
        }
        if (stretch && nlines > 0)
        {
            for (int l = 0; l < nlines; l++)
                lines[l].cross += round_i((double)leftover * (l + 1) / nlines) -
                                  round_i((double)leftover * l / nlines);
            leftover = 0;
        }
        int cursor = 0;
        for (int l = 0; l < nlines; l++)
        {
            lines[l].pos = cursor + dist_before(d, leftover, nlines, l);
            cursor += lines[l].cross + gap_c;
        }
    }

    /* 8. per line: stretch, main/cross positions */
    enum dist jd = justify_dist(n->a.justify);
    for (int l = 0; l < nlines; l++)
    {
        fline *L = &lines[l];
        int total = gap_m * (L->count > 1 ? L->count - 1 : 0);
        for (int i = L->first; i < L->first + L->count; i++)
        {
            fitem *it = &items[i];
            if (it->cross_auto && it->align == TUI_ALIGN_STRETCH)
            {
                int cs = L->cross - it->cs - it->ce;
                it->cross = cs > it->cmax ? it->cmax
                                          : (cs < it->cmin ? it->cmin : cs);
                it->cross = tui__max(0, it->cross);
            }
            total += it->main + it->ms + it->me;
        }
        int free = avail_m - total;
        enum dist d = jd;
        if (free < 0 &&
            (d == DIST_BETWEEN || d == DIST_AROUND || d == DIST_EVENLY))
            d = DIST_START;

        int cursor = 0, idx = 0;
        for (int j = 0; j < L->count; j++)
        {
            int i = L->first + (rev ? L->count - 1 - j : j);
            fitem *it = &items[i];
            int lms = rev ? it->me : it->ms; /* logical start margin */
            /* `cursor` advances by sizes only; dist_before() already returns
             * the cumulative extra space in front of this item */
            int pos = cursor + dist_before(d, free, L->count, idx++);
            int bs = pos + lms; /* logical border-box start */
            it->pos_m = rev ? avail_m - (bs + it->main) : bs;
            cursor += it->ms + it->main + it->me + gap_m;

            int outer_c = it->cross + it->cs + it->ce;
            int space = L->cross - outer_c, off = 0;
            if (it->align == TUI_ALIGN_END)
                off = space;
            else if (it->align == TUI_ALIGN_CENTER)
                off = space / 2;
            it->pos_c = L->pos + off + it->cs;
        }
    }

    /* 9. commit */
    int ext_x = 0, ext_y = 0;
    for (int i = 0; i < cnt; i++)
    {
        fitem *it = &items[i];
        tui_rect r;
        if (row)
            r = (tui_rect){in.left + it->pos_m, in.top + it->pos_c, it->main,
                           it->cross};
        else
            r = (tui_rect){in.left + it->pos_c, in.top + it->pos_m, it->cross,
                           it->main};
        tui_edges mg = it->n->a.margin;
        ext_x = tui__max(ext_x, r.x + r.w + mg.right);
        ext_y = tui__max(ext_y, r.y + r.h + mg.bottom);
        commit_child(n, it->n, r, vis, ox, oy);
    }

    /* absolute children live outside the flow */
    for (tui_node *c = n->first; c; c = c->next)
    {
        if (c->a.display == TUI_DISPLAY_NONE)
        {
            c->flags &= ~NF_LAID_OUT;
            continue;
        }
        if (c->a.position != TUI_POS_ABSOLUTE)
            continue;
        commit_child(n, c, abs_child_rect(n, c, W, H), vis, ox, oy);
    }

    int new_cw = ext_x + in.right, new_ch = ext_y + in.bottom;
    if (vis && n->a.paint_over && (new_cw != n->content_w || new_ch != n->content_h))
        tui__damage_add(n->ctx, (tui_rect){ox, oy, W, H}); /* scrollbars */
    n->content_w = new_cw;
    n->content_h = new_ch;

    /* keep the scroll offset inside the (possibly shrunk) content */
    int mx = tui__max(0, n->content_w - W), my = tui__max(0, n->content_h - H);
    int sx = tui__clamp(n->scroll_x, 0, mx), sy = tui__clamp(n->scroll_y, 0, my);
    if (sx != n->scroll_x || sy != n->scroll_y)
    {
        if (vis)
            tui__damage_add(n->ctx, (tui_rect){ox, oy, W, H});
        n->scroll_x = sx;
        n->scroll_y = sy;
    }

    if (items != item_stack)
        free(items);
    if (lines != line_stack)
        free(lines);
    return H;
}

int tui__flex_height(tui_node *n, int W)
{
    return flex_layout(n, W, -1, false, false, 0, 0);
}

static void layout_node(tui_node *n, bool vis, int ox, int oy)
{
    n->ctx->cur.layout_nodes++;
    n->flags &= ~NF_LAYOUT_DIRTY;
    if (n->first)
    {
        flex_layout(n, n->rect.w, n->rect.h, true, vis, ox, oy);
    }
    else
    {
        n->content_w = n->rect.w;
        n->content_h = n->rect.h;
    }
    if (n->a.on_resize)
        tui__resize_enqueue(n); /* fired after the pass, if the size differs */
}

/* ---- pass driver --------------------------------------------------------- */

typedef struct qent
{
    tui_node *n;
    int depth;
} qent;

static int cmp_depth(const void *a, const void *b)
{
    return ((const qent *)a)->depth - ((const qent *)b)->depth;
}

void tui__layout_pass(tui_ctx *ctx)
{
    if (ctx->nlroots == 0)
        return;

    int n = ctx->nlroots;
    qent *q = malloc((size_t)n * sizeof(*q));
    if (q == NULL)
        abort();
    for (int i = 0; i < n; i++)
    {
        tui_node *b = ctx->lroots[i];
        int d = 0;
        for (tui_node *p = b->parent; p; p = p->parent)
            d++;
        q[i] = (qent){b, d};
        b->flags &= ~NF_QUEUED;
    }
    ctx->nlroots = 0;
    qsort(q, (size_t)n, sizeof(*q), cmp_depth);

    for (int i = 0; i < n; i++)
    {
        tui_node *b = q[i].n;
        if (b->flags & NF_IS_ROOT)
            b->flags |= NF_LAID_OUT; /* the root always has a position */
        /* already handled through an ancestor, or waiting for its parent to
         * give it a rect / for an attach */
        if (!(b->flags & NF_LAYOUT_DIRTY) || !(b->flags & NF_LAID_OUT) ||
            !tui__attached(b))
            continue;
        int ox, oy;
        tui__abs_origin(b, &ox, &oy);
        layout_node(b, tui_is_visible(b), ox, oy);
    }
    free(q);
}
