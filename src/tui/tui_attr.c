#include "tui_internal.h"

#include <stdarg.h>
#include <stdio.h>

/* ------------------------------------------------------------------------ */
/* attr diffing                                                             */
/* ------------------------------------------------------------------------ */

static bool dim_eq(tui_dim a, tui_dim b)
{
    return a.unit == b.unit && (a.unit == TUI_UNIT_AUTO || a.v == b.v);
}

static bool edges_eq(tui_edges a, tui_edges b)
{
    return a.top == b.top && a.right == b.right && a.bottom == b.bottom &&
           a.left == b.left;
}

static bool color_eq(color_t a, color_t b)
{
    return tui__pack(a) == tui__pack(b);
}

/* fields that can change a node's size or its children's positions */
static bool layout_differs(const tui_attr *a, const tui_attr *b)
{
    return a->display != b->display || a->dir != b->dir || a->wrap != b->wrap ||
           a->justify != b->justify || a->align_items != b->align_items ||
           a->align_content != b->align_content || a->gap != b->gap ||
           a->row_gap != b->row_gap || a->col_gap != b->col_gap ||
           a->grow != b->grow || a->shrink != b->shrink ||
           !dim_eq(a->basis, b->basis) || a->align_self != b->align_self ||
           !dim_eq(a->width, b->width) || !dim_eq(a->height, b->height) ||
           !dim_eq(a->min_w, b->min_w) || !dim_eq(a->min_h, b->min_h) ||
           !dim_eq(a->max_w, b->max_w) || !dim_eq(a->max_h, b->max_h) ||
           !edges_eq(a->margin, b->margin) ||
           !edges_eq(a->padding, b->padding) || a->border != b->border ||
           a->position != b->position || !dim_eq(a->left, b->left) ||
           !dim_eq(a->top, b->top) || !dim_eq(a->right, b->right) ||
           !dim_eq(a->bottom, b->bottom) || a->text_wrap != b->text_wrap ||
           a->measure != b->measure;
}

static bool paint_differs(const tui_attr *a, const tui_attr *b)
{
    return !color_eq(a->fg, b->fg) || !color_eq(a->bg, b->bg) ||
           !color_eq(a->border_fg, b->border_fg) || a->attrs != b->attrs ||
           a->text_align != b->text_align || a->paint != b->paint ||
           a->paint_over != b->paint_over;
}

tui_attr tui_get_attr(const tui_node *n)
{
    return n->a;
}

void tui_set_attr(tui_node *n, const tui_attr *attr)
{
    tui_attr next = *attr;
    bool layout = layout_differs(&n->a, &next);
    bool paint = paint_differs(&n->a, &next);

    /* text: compare before touching anything, `attr->text` may alias ours */
    bool text_changed = false;
    if (attr->text != NULL && attr->text != n->text &&
        (strlen(attr->text) != n->text_len ||
         memcmp(attr->text, n->text, n->text_len) != 0))
        text_changed = true;
    bool id_changed = false;
    if (attr->id != n->id_str)
    {
        const char *old = n->id_str ? n->id_str : "";
        const char *nw = attr->id ? attr->id : "";
        id_changed = strcmp(old, nw) != 0;
    }

    if (layout || paint || text_changed)
        tui__damage_node(n); /* old rect, while it still describes the screen */

    if (text_changed)
    {
        size_t len = strlen(attr->text);
        char *copy = malloc(len + 1);
        if (copy == NULL)
            abort();
        memcpy(copy, attr->text, len + 1);
        free(n->text);
        n->text = copy;
        n->text_len = len;
    }

    const char *id_new = attr->id; /* aliasing is handled by id_changed */
    next.id = n->id_str;
    next.text = n->text;
    n->a = next;
    if (id_changed)
        tui__id_set(n, id_new);
    tui__tick_sync(n);

    if (layout || text_changed)
    {
        tui__mark_layout(n);
        if (n->parent)
            tui__mark_layout(n->parent);
    }
}

/* ------------------------------------------------------------------------ */
/* shortcuts                                                                */
/* ------------------------------------------------------------------------ */

void tui_set_text(tui_node *n, const char *utf8)
{
    if (utf8 == NULL)
        utf8 = "";
    if (utf8 == n->text)
        return;
    size_t len = strlen(utf8);
    if (len == n->text_len && memcmp(utf8, n->text, len) == 0)
        return;

    tui__damage_node(n);

    /* Fast path: if the node's intrinsic sizes do not change, the parent's
     * layout cannot change either, so only a repaint is needed.  This keeps
     * clocks, counters and labels O(1) regardless of how many siblings the
     * node has. */
    bool fast = n->kind == TUI_KIND_TEXT && (n->flags & NF_LAID_OUT) &&
                !(n->flags & NF_LAYOUT_DIRTY);
    int om = 0, oh = 0, omin = 0;
    if (fast)
    {
        om = tui__max_content_w(n);
        oh = tui__height_for_width(n, n->rect.w);
        omin = tui__text_min_width(n);
    }

    char *copy = malloc(len + 1);
    if (copy == NULL)
        abort();
    memcpy(copy, utf8, len + 1);
    free(n->text);
    n->text = copy;
    n->text_len = len;
    n->a.text = copy;

    if (fast)
    {
        tui__invalidate_chain(n);
        if (tui__max_content_w(n) == om &&
            tui__height_for_width(n, n->rect.w) == oh &&
            tui__text_min_width(n) == omin)
            return;
    }
    tui__mark_layout(n); /* climbs to the nearest fixed-size ancestor */
}

void tui_set_textf(tui_node *n, const char *fmt, ...)
{
    char stackbuf[256];
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int len = vsnprintf(stackbuf, sizeof(stackbuf), fmt, ap);
    va_end(ap);
    if (len < 0)
    {
        va_end(ap2);
        return;
    }
    if ((size_t)len < sizeof(stackbuf))
    {
        va_end(ap2);
        tui_set_text(n, stackbuf);
        return;
    }
    char *heap = malloc((size_t)len + 1);
    if (heap == NULL)
        abort();
    vsnprintf(heap, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    tui_set_text(n, heap);
    free(heap);
}

const char *tui_get_text(const tui_node *n)
{
    return n->text;
}

void tui_set_fg(tui_node *n, color_t c)
{
    if (color_eq(n->a.fg, c))
        return;
    tui__damage_node(n);
    n->a.fg = c;
}

void tui_set_bg(tui_node *n, color_t c)
{
    if (color_eq(n->a.bg, c))
        return;
    tui__damage_node(n);
    n->a.bg = c;
}

void tui_set_visible(tui_node *n, bool visible)
{
    tui_attr a = n->a;
    a.display = visible ? TUI_DISPLAY_FLEX : TUI_DISPLAY_NONE;
    tui_set_attr(n, &a);
}

void tui_set_size(tui_node *n, tui_dim w, tui_dim h)
{
    tui_attr a = n->a;
    a.width = w;
    a.height = h;
    tui_set_attr(n, &a);
}

void tui_set_grow(tui_node *n, float grow)
{
    tui_attr a = n->a;
    a.grow = grow;
    tui_set_attr(n, &a);
}

void tui_set_userdata(tui_node *n, void *ud)
{
    n->a.userdata = ud;
}

void *tui_get_userdata(const tui_node *n)
{
    return n->a.userdata;
}

/* ------------------------------------------------------------------------ */
/* scrolling / invalidation                                                 */
/* ------------------------------------------------------------------------ */

static void scroll_limits(const tui_node *n, int *mx, int *my)
{
    if (n->flags & NF_LAID_OUT)
    {
        *mx = tui__max(0, n->content_w - n->rect.w);
        *my = tui__max(0, n->content_h - n->rect.h);
    }
    else
    {
        *mx = *my = 1 << 30;
    }
}

void tui_set_scroll(tui_node *n, int x, int y)
{
    int mx, my;
    scroll_limits(n, &mx, &my);
    x = tui__clamp(x, 0, mx);
    y = tui__clamp(y, 0, my);
    if (x == n->scroll_x && y == n->scroll_y)
        return;
    tui__damage_node(n); /* children moved inside the box */
    n->scroll_x = x;
    n->scroll_y = y;
}

void tui_scroll_by(tui_node *n, int dx, int dy)
{
    tui_set_scroll(n, n->scroll_x + dx, n->scroll_y + dy);
}

tui_size tui_get_scroll(const tui_node *n)
{
    return (tui_size){n->scroll_x, n->scroll_y};
}

tui_size tui_get_content_size(const tui_node *n)
{
    int b = tui__border_w(n);
    return (tui_size){tui__max(0, n->content_w - 2 * b),
                      tui__max(0, n->content_h - 2 * b)};
}

void tui_invalidate(tui_node *n)
{
    tui__damage_node(n);
}

void tui_invalidate_rect(tui_node *n, tui_rect r)
{
    if (!(n->flags & NF_LAID_OUT) || !tui_is_visible(n))
        return;
    tui_rect c = tui_get_content_rect(n);
    r.x += c.x;
    r.y += c.y;
    r = tui__rect_intersect(r, c);
    tui__damage_add(n->ctx, r);
}

/* ------------------------------------------------------------------------ */
/* declarative builder                                                      */
/* ------------------------------------------------------------------------ */

tui_node *tui_cursor(tui_ctx *ctx)
{
    return ctx->bdepth > 0 ? ctx->bstack[ctx->bdepth - 1] : ctx->root;
}

tui_node *tui_last(tui_ctx *ctx)
{
    return ctx->blast;
}

static void bpush(tui_ctx *ctx, tui_node *n)
{
    if (ctx->bdepth == ctx->bcap)
    {
        ctx->bcap = ctx->bcap ? ctx->bcap * 2 : 16;
        ctx->bstack = realloc(ctx->bstack, (size_t)ctx->bcap * sizeof(*ctx->bstack));
        if (ctx->bstack == NULL)
            abort();
    }
    ctx->bstack[ctx->bdepth++] = n;
}

tui_node *tui__open(tui_ctx *ctx, tui_kind kind, const tui_attr *attr)
{
    tui_node *n = tui_node_new(ctx, kind, attr);
    tui_append(tui_cursor(ctx), n);
    bpush(ctx, n);
    ctx->blast = n;
    return n;
}

void tui__close(tui_ctx *ctx)
{
    if (ctx->bdepth > 0)
        ctx->bdepth--;
}

tui_node *tui_build_push(tui_ctx *ctx, tui_node *under)
{
    bpush(ctx, under);
    return under;
}

void tui_build_pop(tui_ctx *ctx, tui_node *prev)
{
    (void)prev;
    tui__close(ctx);
}
