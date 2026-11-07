#ifndef __TUI_FUZZ_H
#define __TUI_FUZZ_H

/* Differential fuzzer for tui.
 *
 * Random trees are mutated through the public API (text, attributes, add /
 * remove / move subtrees, visibility, scroll, resize, ...).  After every frame:
 *
 *   1. the emulated terminal (tui_vt.h) must equal the renderer's front buffer
 *      and the stream must never print off-screen or use unknown sequences;
 *   2. every few frames the whole tree is marked never-laid-out and fully
 *      repainted: the from-scratch layout must equal the incremental layout,
 *      and the from-scratch paint must equal the incrementally painted buffer.
 *
 * tui_fuzz_run() returns 0 on success and prints a diagnostic on failure. */

#include "tui_vt.h"


static char fz_last_buf[1 << 16];
static size_t fz_last_len;
static void fz_tee_cb(const char *d, size_t n, void *ud)
{
    fz_last_len = n < sizeof(fz_last_buf) ? n : sizeof(fz_last_buf);
    memcpy(fz_last_buf, d, fz_last_len);
    vt_feed((vt *)ud, d, n);
}

static uint32_t fz_rng_state = 1;
static uint32_t fz_rnd(void)
{
    fz_rng_state ^= fz_rng_state << 13;
    fz_rng_state ^= fz_rng_state >> 17;
    fz_rng_state ^= fz_rng_state << 5;
    return fz_rng_state;
}
static int fz_R(int n) { return n <= 0 ? 0 : (int)(fz_rnd() % (uint32_t)n); }
static bool fz_chance(int pct) { return fz_R(100) < pct; }

static const char *fz_texts[] = {
    "", "a", "hello", "hello world", "the quick brown fox jumps over",
    "日本語テキスト", "ab日本cd", "line1\nline2", "x\n\ny", "  padded  ",
    "averyveryverylongwordthatwillnotfitanywhere", "→ arrow ←", "tab\there",
};
#define FZ_NTEXTS (int)(sizeof(fz_texts) / sizeof(fz_texts[0]))

static tui_dim fz_rdim(void)
{
    switch (fz_R(5))
    {
    case 0: return TUI_CELLS(fz_R(14));
    case 1: return TUI_PCT(fz_R(101));
    default: return TUI_AUTO;
    }
}

static color_t fz_rcolor(void)
{
    if (fz_chance(50))
        return COLOR_NONE;
    return COLOR(fz_R(256), fz_R(256), fz_R(256));
}

static void fz_rand_attr(tui_attr *a, bool text)
{
    a->dir = (tui_direction)fz_R(4);
    a->wrap = fz_chance(25) ? TUI_WRAP : TUI_NOWRAP;
    a->justify = (tui_justify)fz_R(6);
    a->align_items = (tui_align)fz_R(5);
    a->align_content = (tui_align_content)fz_R(6);
    a->gap = fz_chance(30) ? fz_R(3) : 0;
    a->row_gap = fz_chance(10) ? fz_R(3) : 0;
    a->col_gap = fz_chance(10) ? fz_R(3) : 0;
    a->grow = fz_chance(40) ? (float)fz_R(3) : (fz_chance(10) ? 0.5f : 0);
    a->shrink = fz_chance(10) ? TUI_NO_SHRINK : (fz_chance(10) ? 2.0f : 0);
    a->basis = fz_chance(15) ? TUI_CELLS(fz_R(10)) : TUI_AUTO;
    a->align_self = fz_chance(30) ? (tui_align)fz_R(5) : TUI_ALIGN_AUTO;
    a->width = fz_rdim();
    a->height = fz_rdim();
    a->min_w = fz_chance(10) ? TUI_CELLS(fz_R(6)) : TUI_AUTO;
    a->min_h = fz_chance(10) ? TUI_CELLS(fz_R(4)) : TUI_AUTO;
    a->max_w = fz_chance(10) ? TUI_CELLS(1 + fz_R(20)) : TUI_AUTO;
    a->max_h = fz_chance(10) ? TUI_CELLS(1 + fz_R(10)) : TUI_AUTO;
    a->margin = fz_chance(25) ? (tui_edges){fz_R(2), fz_R(2), fz_R(2), fz_R(2)} : (tui_edges){0};
    a->padding = fz_chance(25) ? (tui_edges){fz_R(2), fz_R(2), fz_R(2), fz_R(2)} : (tui_edges){0};
    a->border = fz_chance(25) ? (tui_border_style)(1 + fz_R(5)) : TUI_BORDER_NONE;
    a->position = fz_chance(8) ? TUI_POS_ABSOLUTE : TUI_POS_RELATIVE;
    a->left = fz_chance(40) ? TUI_CELLS(fz_R(10)) : TUI_AUTO;
    a->top = fz_chance(40) ? TUI_CELLS(fz_R(6)) : TUI_AUTO;
    a->right = fz_chance(20) ? TUI_CELLS(fz_R(10)) : TUI_AUTO;
    a->bottom = fz_chance(20) ? TUI_CELLS(fz_R(6)) : TUI_AUTO;
    a->fg = fz_rcolor();
    a->bg = fz_chance(35) ? fz_rcolor() : COLOR_NONE;
    a->border_fg = fz_rcolor();
    a->attrs = (uint16_t)(fz_chance(30) ? fz_R(64) : 0);
    a->display = fz_chance(5) ? TUI_DISPLAY_NONE : TUI_DISPLAY_FLEX;
    if (text)
    {
        a->text_wrap = (tui_text_wrap)fz_R(3);
        a->text_align = (tui_text_align)fz_R(3);
    }
}

static tui_node *fz_make_tree(tui_ctx *ctx, int depth)
{
    bool leaf = depth <= 0 || fz_chance(40);
    tui_attr a = {0};
    fz_rand_attr(&a, leaf);
    if (leaf && fz_chance(80))
    {
        a.text = fz_texts[fz_R(FZ_NTEXTS)];
        return tui_node_new(ctx, TUI_KIND_TEXT, &a);
    }
    tui_node *n = tui_node_new(ctx, TUI_KIND_BOX, &a);
    if (!leaf)
    {
        int k = fz_R(5);
        for (int i = 0; i < k; i++)
            tui_append(n, fz_make_tree(ctx, depth - 1));
    }
    return n;
}

static int fz_collect(tui_node *n, tui_node **out, int cap, int c)
{
    if (c < cap)
        out[c] = n;
    c++;
    for (tui_node *k = tui_first_child(n); k; k = tui_next_sibling(k))
        c = fz_collect(k, out, cap, c);
    return c;
}

static int fz_is_ancestor(tui_node *a, tui_node *n)
{
    for (; n; n = tui_parent(n))
        if (n == a)
            return 1;
    return 0;
}

static void fz_mutate(tui_ctx *ctx)
{
    tui_node *all[4096];
    int n = fz_collect(tui_root(ctx), all, 4096, 0);
    if (n > 4096)
        n = 4096;
    tui_node *t = all[fz_R(n)];

    switch (fz_R(14))
    {
    case 0:
    case 1:
        if (tui_node_kind(t) == TUI_KIND_TEXT)
            tui_set_text(t, fz_texts[fz_R(FZ_NTEXTS)]);
        break;
    case 2:
    case 3: {
        if (t == tui_root(ctx))
            break;
        tui_attr a = tui_get_attr(t);
        tui_attr r = {0};
        fz_rand_attr(&r, tui_node_kind(t) == TUI_KIND_TEXT);
        /* fz_mutate a random subset of fields */
        if (fz_chance(50)) { a.dir = r.dir; a.wrap = r.wrap; }
        if (fz_chance(50)) { a.justify = r.justify; a.align_items = r.align_items; }
        if (fz_chance(50)) { a.grow = r.grow; a.shrink = r.shrink; a.basis = r.basis; }
        if (fz_chance(50)) { a.width = r.width; }
        if (fz_chance(50)) { a.height = r.height; }
        if (fz_chance(30)) { a.min_w = r.min_w; a.max_w = r.max_w; a.min_h = r.min_h; a.max_h = r.max_h; }
        if (fz_chance(30)) { a.margin = r.margin; a.padding = r.padding; a.border = r.border; }
        if (fz_chance(30)) { a.gap = r.gap; a.align_content = r.align_content; a.align_self = r.align_self; }
        if (fz_chance(40)) { a.fg = r.fg; a.bg = r.bg; a.attrs = r.attrs; }
        if (fz_chance(15)) { a.position = r.position; a.left = r.left; a.top = r.top; a.right = r.right; a.bottom = r.bottom; }
        if (fz_chance(15)) { a.display = r.display; }
        if (fz_chance(20)) { a.text_wrap = r.text_wrap; a.text_align = r.text_align; }
        tui_set_attr(t, &a);
        break;
    }
    case 4:
        tui_set_bg(t, fz_rcolor());
        break;
    case 5:
        tui_set_fg(t, fz_rcolor());
        break;
    case 6:
        if (t != tui_root(ctx))
            tui_set_visible(t, fz_chance(70));
        break;
    case 7:
    case 8:
        if (tui_node_kind(t) == TUI_KIND_BOX)
            tui_append(t, fz_make_tree(ctx, 2));
        break;
    case 9:
        if (t != tui_root(ctx))
            tui_node_free(t);
        break;
    case 10: { /* move */
        tui_node *dst = all[fz_R(n)];
        if (t != tui_root(ctx) && tui_node_kind(dst) == TUI_KIND_BOX &&
            !fz_is_ancestor(t, dst))
        {
            if (fz_chance(50))
                tui_append(dst, t);
            else
                tui_insert_at(dst, t, fz_R(4));
        }
        break;
    }
    case 11:
        tui_set_scroll(t, fz_R(6), fz_R(6));
        break;
    case 12:
        tui_invalidate(t);
        break;
    case 13:
        if (t != tui_root(ctx) && tui_node_kind(t) == TUI_KIND_BOX)
            tui_clear_children(t);
        break;
    }
}

/* make the whole tree look never-laid-out so the next pass is from scratch */
static void fz_scramble(tui_node *n)
{
    if (!(n->flags & NF_IS_ROOT))
        n->flags &= ~NF_LAID_OUT;
    n->flags |= NF_LAYOUT_DIRTY;
    tui__invalidate_caches(n);
    for (tui_node *c = n->first; c; c = c->next)
        fz_scramble(c);
}

static void fz_dump_tree(tui_node *n, int depth)
{
    tui_rect r = n->rect;
    fprintf(stderr, "%*s%s rel(%d,%d %dx%d) laid=%d disp=%d pos=%d bg=%x fg=%x at=%x scroll=(%d,%d) text=\"%.20s\"\n",
            depth * 2, "", n->kind == TUI_KIND_TEXT ? "T" : "B", r.x, r.y, r.w, r.h,
            !!(n->flags & NF_LAID_OUT), n->a.display, n->a.position, tui__pack(n->a.bg), tui__pack(n->a.fg),
            n->a.attrs, n->scroll_x, n->scroll_y, n->kind == TUI_KIND_TEXT ? n->text : "");
    for (tui_node *c = n->first; c; c = c->next)
        fz_dump_tree(c, depth + 1);
}

static int fz_snap_rects(tui_node *n, tui_rect *out, int cap, int c)
{
    if (c < cap)
        out[c] = n->rect;
    c++;
    for (tui_node *k = n->first; k; k = k->next)
        c = fz_snap_rects(k, out, cap, c);
    return c;
}

static int tui_fuzz_run(int seeds, int steps)
{
    long total_frames = 0;

    for (int seed = 1; seed <= seeds; seed++)
    {
        fz_rng_state = (uint32_t)seed * 2654435761u + 12345u;
        int W = 8 + fz_R(50), H = 4 + fz_R(20);
        vt t = vt_new(W, H);
        tui_options o = {.write = fz_tee_cb, .write_ud = &t,
                         .color_mode = fz_chance(30) ? TUI_COLOR_256 : TUI_COLOR_TRUE,
                         .sync_output = fz_chance(50)};
        tui_ctx *ctx = tui_create(W, H, &o);
        int k = 1 + fz_R(4);
        for (int i = 0; i < k; i++)
            tui_append(tui_root(ctx), fz_make_tree(ctx, 4));

        for (int step = 0; step < steps; step++)
        {
            int ops = 1 + fz_R(4);
            for (int i = 0; i < ops; i++)
                fz_mutate(ctx);

            if (fz_chance(4))
            {
                int nW = 6 + fz_R(50), nH = 3 + fz_R(20);
                bool same = nW == W && nH == H;
                W = nW;
                H = nH;
                tui_resize(ctx, W, H);
                if (!same)
                    vt_resize(&t, W, H);
                o.write_ud = &t;
                ctx->opt.write_ud = &t;
            }

            tui_render(ctx);
            total_frames++;

#define FAIL(msg)                                                              \
    do                                                                         \
    {                                                                          \
        fprintf(stderr, "FAIL seed=%d step=%d: %s\n", seed, step, msg);        \
        vt_dump(&t);                                                           \
        return 1;                                                              \
    } while (0)

            if (t.errors)
                { fprintf(stderr, "why: %s\n", t.why); FAIL("vt errors"); }
            int d = vt_diff_buf(&t, ctx->front, ctx->opt.color_mode == TUI_COLOR_256);
            if (d >= 0)
            {
                vt_cell *a = &t.cells[d];
                tui_cell *b = &ctx->front[d];
                fprintf(stderr, "cell (%d,%d): term cp=%x w=%d fg=%x bg=%x at=%x | front cp=%x w=%d fg=%x bg=%x at=%x  (W=%d H=%d)\n",
                        d % t.W, d / t.W, a->cp, a->w, a->fg, a->bg, a->attrs, b->cp, b->w, b->fg, b->bg, b->attrs, t.W, t.H);
                for (int dx = -3; dx <= 3; dx++)
                {
                    int xx = d % t.W + dx, yy = d / t.W;
                    if (xx < 0 || xx >= t.W) continue;
                    vt_cell *ta = &t.cells[yy * t.W + xx];
                    tui_cell *fb = &ctx->front[yy * t.W + xx], *bb = &ctx->back[yy * t.W + xx];
                    fprintf(stderr, "  x=%d term(cp=%x w=%d) front(cp=%x w=%d) back(cp=%x w=%d)\n", xx, ta->cp, ta->w, fb->cp, fb->w, bb->cp, bb->w);
                }
                fprintf(stderr, "last frame bytes (%zu): ", fz_last_len);
                for (size_t q = 0; q < fz_last_len; q++) { unsigned char c = (unsigned char)fz_last_buf[q]; if (c == 27) fprintf(stderr, "<E>"); else if (c < 32) fprintf(stderr, "<%02x>", c); else if (c > 126) fputc(c, stderr); else fputc(c, stderr); }
                fprintf(stderr, "\n");
                FAIL("terminal != front");
            }
            if (memcmp(ctx->back, ctx->front, (size_t)ctx->width * ctx->height * sizeof(tui_cell)))
                FAIL("back != front");

            /* paint equivalence + layout equivalence against from-scratch */
            if (step % 3 == 0)
            {
                size_t cells = (size_t)ctx->width * ctx->height;
                tui_cell *snap = malloc(cells * sizeof(tui_cell));
                memcpy(snap, ctx->back, cells * sizeof(tui_cell));
                tui_rect rects[4096], rects2[4096];
                int nr = fz_snap_rects(tui_root(ctx), rects, 4096, 0);

                fz_scramble(tui_root(ctx));
                tui__mark_layout(tui_root(ctx));
                tui_force_redraw(ctx);
                tui_render(ctx);

                int nr2 = fz_snap_rects(tui_root(ctx), rects2, 4096, 0);
                if (nr != nr2)
                    FAIL("node count changed");
                for (int i = 0; i < nr && i < 4096; i++)
                    if (memcmp(&rects[i], &rects2[i], sizeof(tui_rect)))
                    {
                        fprintf(stderr, "node #%d: inc (%d,%d,%d,%d) vs scratch (%d,%d,%d,%d)\n", i,
                                rects[i].x, rects[i].y, rects[i].w, rects[i].h,
                                rects2[i].x, rects2[i].y, rects2[i].w, rects2[i].h);
                        FAIL("incremental layout != from-scratch layout");
                    }
                if (memcmp(snap, ctx->back, cells * sizeof(tui_cell)))
                {
                    for (size_t i = 0; i < cells; i++)
                        if (memcmp(&snap[i], &ctx->back[i], sizeof(tui_cell)))
                        {
                            fprintf(stderr, "cell (%d,%d): inc cp=%x w=%d fg=%x bg=%x at=%x | full cp=%x w=%d fg=%x bg=%x at=%x\n",
                                    (int)(i % ctx->width), (int)(i / ctx->width),
                                    snap[i].cp, snap[i].w, snap[i].fg, snap[i].bg, snap[i].attrs,
                                    ctx->back[i].cp, ctx->back[i].w, ctx->back[i].fg, ctx->back[i].bg, ctx->back[i].attrs);
                            break;
                        }
                    free(snap);
                    fz_dump_tree(tui_root(ctx), 0);
                    FAIL("incremental paint != full repaint");
                }
                free(snap);
                if (t.errors)
                    FAIL("vt errors after full");
                if (vt_diff_buf(&t, ctx->front, ctx->opt.color_mode == TUI_COLOR_256) >= 0)
                    FAIL("terminal != front after full");
            }
        }
        tui_destroy(ctx);
        vt_free(&t);
    }
    (void)total_frames;
    return 0;
}

#endif /* __TUI_FUZZ_H */
