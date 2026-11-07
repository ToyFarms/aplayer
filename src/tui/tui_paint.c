#include "tui_internal.h"

/* ------------------------------------------------------------------------ */
/* back buffer primitives                                                   */
/* ------------------------------------------------------------------------ */

static const tui_cell BLANK = {' ', 0, 0, 0, 1};

static inline tui_cell *cell_at(tui_ctx *ctx, int x, int y)
{
    return &ctx->back[(size_t)y * ctx->width + x];
}

/* About to overwrite cell (x,y): if it belongs to a wide glyph, the other
 * half would be orphaned, so blank it. */
static void orphan_fix(tui_ctx *ctx, int x, int y)
{
    tui_cell *c = cell_at(ctx, x, y);
    if (c->w == 2 && x + 1 < ctx->width)
    {
        tui_cell *p = c + 1;
        *p = BLANK;
        p->bg = c->bg;
    }
    else if (c->w == 0 && x > 0)
    {
        tui_cell *p = c - 1;
        if (p->w == 2)
        {
            *p = BLANK;
            p->bg = c->bg;
        }
    }
}

void tui__buf_clear(tui_ctx *ctx, tui_rect r)
{
    r = tui__rect_intersect(r, (tui_rect){0, 0, ctx->width, ctx->height});
    for (int y = r.y; y < r.y + r.h; y++)
    {
        tui_cell *row = cell_at(ctx, r.x, y);
        for (int x = 0; x < r.w; x++)
            row[x] = BLANK;
    }
}

/* Caller guarantees 0 <= x, x + w <= width, 0 <= y < height. */
void tui__buf_put(tui_ctx *ctx, int x, int y, uint32_t cp, int w, uint32_t fg,
                  uint32_t bg, uint16_t attrs, bool keep_bg)
{
    for (int k = 0; k < w; k++)
        orphan_fix(ctx, x + k, y);
    /* a terminal paints both halves of a wide glyph with ONE pen, so a
     * transparent background is resolved once, from the lead cell */
    uint32_t nbg = (keep_bg && bg == 0) ? cell_at(ctx, x, y)->bg : bg;
    for (int k = 0; k < w; k++)
    {
        tui_cell *c = cell_at(ctx, x + k, y);
        c->cp = k == 0 ? cp : 0;
        c->fg = fg;
        c->bg = nbg;
        c->attrs = attrs;
        c->w = k == 0 ? (uint16_t)w : 0;
    }
}

/* ------------------------------------------------------------------------ */
/* canvas                                                                   */
/* ------------------------------------------------------------------------ */

int tui_canvas_width(const tui_canvas *c)
{
    return c->box.w;
}

int tui_canvas_height(const tui_canvas *c)
{
    return c->box.h;
}

static inline bool in_clip(const tui_canvas *c, int sx, int sy)
{
    return sx >= c->clip.x && sx < c->clip.x + c->clip.w && sy >= c->clip.y &&
           sy < c->clip.y + c->clip.h;
}

int tui_canvas_put(tui_canvas *c, int x, int y, uint32_t cp, tui_pen pen)
{
    int w = tui__cp_width(cp);
    if (w <= 0)
        return 0;
    if (cp == '\t')
        cp = ' ';
    int sx = c->box.x + x, sy = c->box.y + y;
    uint32_t fg = tui__pack(pen.fg), bg = tui__pack(pen.bg);
    bool keep = pen.bg.a == 0;

    if (w == 1)
    {
        if (!in_clip(c, sx, sy))
            return 0;
        tui__buf_put(c->ctx, sx, sy, cp, 1, fg, bg, pen.attrs, keep);
        return 1;
    }

    bool a = in_clip(c, sx, sy), b = in_clip(c, sx + 1, sy);
    if (a && b && sx + 1 < c->ctx->width)
    {
        tui__buf_put(c->ctx, sx, sy, cp, 2, fg, bg, pen.attrs, keep);
        return 2;
    }

    /* Legal for the node (both halves inside its real clip) but one half is
     * outside the damage region: the region must grow to hold the glyph. */
    tui_canvas sc = {c->ctx, c->box, c->sclip, c->sclip};
    if ((a || b) && in_clip(&sc, sx, sy) && in_clip(&sc, sx + 1, sy) &&
        sx + 1 < c->ctx->width)
    {
        tui_rect g = {sx, sy, 2, 1};
        c->ctx->cut_rect = c->ctx->cut ? tui__rect_union(c->ctx->cut_rect, g) : g;
        c->ctx->cut = true;
        return 0;
    }
    if (!a && !b)
        return 0;

    /* a wide glyph never shows half: draw blanks where it is really cut */
    if (a)
        tui__buf_put(c->ctx, sx, sy, ' ', 1, fg, bg, pen.attrs, keep);
    if (b)
        tui__buf_put(c->ctx, sx + 1, sy, ' ', 1, fg, bg, pen.attrs, keep);
    return 0;
}

int tui_canvas_textn(tui_canvas *c, int x, int y, const char *s, size_t len,
                     tui_pen pen)
{
    if (y + c->box.y < c->clip.y || y + c->box.y >= c->clip.y + c->clip.h)
    {
        /* still report the width so callers can lay out */
        return tui__text_width(s, len);
    }
    int cols = 0;
    size_t i = 0;
    int right = c->clip.x + c->clip.w;
    while (i < len)
    {
        uint32_t cp;
        i += tui__utf8_decode(s + i, len - i, &cp);
        int w = tui__cp_width(cp);
        if (w == 0)
            continue;
        if (c->box.x + x + cols >= right)
        {
            cols += w;
            continue; /* keep counting; nothing more can be visible */
        }
        tui_canvas_put(c, x + cols, y, cp, pen);
        cols += w;
    }
    return cols;
}

int tui_canvas_text(tui_canvas *c, int x, int y, const char *s, tui_pen pen)
{
    return tui_canvas_textn(c, x, y, s, strlen(s), pen);
}

void tui_canvas_fill(tui_canvas *c, tui_rect r, uint32_t cp, tui_pen pen)
{
    if (tui__cp_width(cp) != 1)
        cp = ' ';
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            tui_canvas_put(c, x, y, cp, pen);
}

void tui_canvas_hline(tui_canvas *c, int x, int y, int len, uint32_t cp,
                      tui_pen pen)
{
    for (int i = 0; i < len; i++)
        tui_canvas_put(c, x + i, y, cp, pen);
}

void tui_canvas_vline(tui_canvas *c, int x, int y, int len, uint32_t cp,
                      tui_pen pen)
{
    for (int i = 0; i < len; i++)
        tui_canvas_put(c, x, y + i, cp, pen);
}

/* ------------------------------------------------------------------------ */
/* border                                                                   */
/* ------------------------------------------------------------------------ */

static void draw_border(tui_ctx *ctx, const tui_node *n, tui_rect r,
                        tui_rect clip, tui_rect sclip, color_t fgc,
                        uint16_t attrs)
{
    static const uint32_t sets[][6] = {
        {0, 0, 0, 0, 0, 0},
        {0x250C, 0x2510, 0x2514, 0x2518, 0x2500, 0x2502}, /* single  */
        {0x256D, 0x256E, 0x2570, 0x256F, 0x2500, 0x2502}, /* rounded */
        {0x2554, 0x2557, 0x255A, 0x255D, 0x2550, 0x2551}, /* double  */
        {0x250F, 0x2513, 0x2517, 0x251B, 0x2501, 0x2503}, /* heavy   */
        {'+', '+', '+', '+', '-', '|'},                   /* ascii   */
    };
    if (r.w < 2 || r.h < 2)
        return;
    const uint32_t *s = sets[n->a.border];
    tui_canvas cv = {ctx, (tui_rect){r.x, r.y, r.w, r.h}, clip, sclip};
    tui_pen pen = {fgc, COLOR_NONE, attrs};

    tui_canvas_put(&cv, 0, 0, s[0], pen);
    tui_canvas_put(&cv, r.w - 1, 0, s[1], pen);
    tui_canvas_put(&cv, 0, r.h - 1, s[2], pen);
    tui_canvas_put(&cv, r.w - 1, r.h - 1, s[3], pen);
    tui_canvas_hline(&cv, 1, 0, r.w - 2, s[4], pen);
    tui_canvas_hline(&cv, 1, r.h - 1, r.w - 2, s[4], pen);
    tui_canvas_vline(&cv, 0, 1, r.h - 2, s[5], pen);
    tui_canvas_vline(&cv, r.w - 1, 1, r.h - 2, s[5], pen);
}

/* ------------------------------------------------------------------------ */
/* tree walk                                                                */
/* ------------------------------------------------------------------------ */

static void paint_node(tui_ctx *ctx, tui_node *n, int ox, int oy,
                       tui_rect clip, tui_rect sclip, color_t inh_fg,
                       uint16_t inh_attrs)
{
    if (n->a.display == TUI_DISPLAY_NONE)
        return;
    tui_rect rect = {ox, oy, n->rect.w, n->rect.h};
    tui_rect vis = tui__rect_intersect(rect, clip);
    if (tui__rect_empty(vis))
        return;
    tui_rect svis = tui__rect_intersect(rect, sclip);
    ctx->cur.painted_nodes++;

    color_t fg = n->a.fg.a ? n->a.fg : inh_fg;
    uint16_t attrs = (uint16_t)(inh_attrs | n->a.attrs);

    if (n->a.bg.a)
    {
        uint32_t bg = tui__pack(n->a.bg);
        for (int y = vis.y; y < vis.y + vis.h; y++)
            for (int x = vis.x; x < vis.x + vis.w; x++)
                tui__buf_put(ctx, x, y, ' ', 1, 0, bg, 0, false);
    }

    int b = tui__border_w(n);
    if (b)
        draw_border(ctx, n, rect, vis, svis,
                    n->a.border_fg.a ? n->a.border_fg : fg, attrs);

    tui_edges in = tui__insets(n);
    tui_rect box = {rect.x + in.left, rect.y + in.top,
                    tui__max(0, rect.w - in.left - in.right),
                    tui__max(0, rect.h - in.top - in.bottom)};
    tui_rect cclip = tui__rect_intersect(vis, box);
    if (!tui__rect_empty(cclip) && (n->kind == TUI_KIND_TEXT || n->a.paint))
    {
        tui_canvas cv = {ctx, box, cclip, tui__rect_intersect(svis, box)};
        tui_pen pen = {fg, COLOR_NONE, attrs};
        if (n->kind == TUI_KIND_TEXT)
            tui__text_paint(n, &cv, pen);
        if (n->a.paint)
            n->a.paint(n, &cv, n->a.userdata);
    }

    tui_rect pad = {rect.x + b, rect.y + b, tui__max(0, rect.w - 2 * b),
                    tui__max(0, rect.h - 2 * b)};
    tui_rect kclip = tui__rect_intersect(vis, pad);
    tui_rect skclip = tui__rect_intersect(svis, pad);
    if (tui__rect_empty(kclip))
        return;

    /* normal flow first, positioned (absolute) children on top */
    for (int pass = 0; pass < 2; pass++)
    {
        for (tui_node *c = n->first; c; c = c->next)
        {
            bool abs = c->a.position == TUI_POS_ABSOLUTE;
            if (abs != (pass == 1))
                continue;
            paint_node(ctx, c, ox + c->rect.x - n->scroll_x,
                       oy + c->rect.y - n->scroll_y, kclip, skclip, fg, attrs);
        }
    }

    if (n->a.paint_over)
    {
        tui_canvas cv = {ctx, pad, kclip, skclip};
        n->a.paint_over(n, &cv, n->a.userdata);
    }
}

/* widen a rect so it never cuts a wide glyph that is currently in `back` */
static void widen_for_wide(tui_ctx *ctx, tui_rect *r)
{
    for (;;)
    {
        bool left = false, right = false;
        for (int y = r->y; y < r->y + r->h; y++)
        {
            tui_cell *row = cell_at(ctx, 0, y);
            if (r->x > 0 && row[r->x].w == 0)
                left = true;
            if (r->x + r->w < ctx->width && row[r->x + r->w - 1].w == 2)
                right = true;
        }
        if (!left && !right)
            return;
        if (left)
        {
            r->x--;
            r->w++;
        }
        if (right)
            r->w++;
    }
}

void tui__paint_region(tui_ctx *ctx, tui_rect *region)
{
    tui_rect screen = {0, 0, ctx->width, ctx->height};
    *region = tui__rect_intersect(*region, screen);
    if (tui__rect_empty(*region))
        return;

    for (int guard = 0; guard < 8; guard++)
    {
        widen_for_wide(ctx, region);
        ctx->cut = false;
        tui__buf_clear(ctx, *region);
        paint_node(ctx, ctx->root, 0, 0, *region, screen, COLOR_NONE, 0);
        if (!ctx->cut)
            return;
        *region = tui__rect_intersect(tui__rect_union(*region, ctx->cut_rect),
                                      screen);
    }
}
