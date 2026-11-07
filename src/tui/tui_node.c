#include "tui_internal.h"

#include <assert.h>
#include <stdio.h>

/* ------------------------------------------------------------------------ */
/* small helpers                                                            */
/* ------------------------------------------------------------------------ */

tui_rect tui__rect_intersect(tui_rect a, tui_rect b)
{
    int x0 = tui__max(a.x, b.x), y0 = tui__max(a.y, b.y);
    int x1 = tui__min(a.x + a.w, b.x + b.w);
    int y1 = tui__min(a.y + a.h, b.y + b.h);
    if (x1 <= x0 || y1 <= y0)
        return (tui_rect){x0, y0, 0, 0};
    return (tui_rect){x0, y0, x1 - x0, y1 - y0};
}

tui_rect tui__rect_union(tui_rect a, tui_rect b)
{
    if (tui__rect_empty(a))
        return b;
    if (tui__rect_empty(b))
        return a;
    int x0 = tui__min(a.x, b.x), y0 = tui__min(a.y, b.y);
    int x1 = tui__max(a.x + a.w, b.x + b.w);
    int y1 = tui__max(a.y + a.h, b.y + b.h);
    return (tui_rect){x0, y0, x1 - x0, y1 - y0};
}

uint32_t tui__pack(color_t c)
{
    if (c.a == 0)
        return 0;
    return 0xFF000000u | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

static char *dup_n(const char *s, size_t n)
{
    char *p = malloc(n + 1);
    if (p == NULL)
        abort();
    if (n)
        memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

tui_edges tui__insets(const tui_node *n)
{
    int b = tui__border_w(n);
    return (tui_edges){n->a.padding.top + b, n->a.padding.right + b,
                       n->a.padding.bottom + b, n->a.padding.left + b};
}

/* ------------------------------------------------------------------------ */
/* id table                                                                 */
/* ------------------------------------------------------------------------ */

#define ID_TOMB ((tui_node *)1)

static uint32_t id_hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++)
        h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

static void ids_insert_raw(tui_node **tab, int cap, tui_node *n)
{
    uint32_t i = id_hash(n->id_str) & (uint32_t)(cap - 1);
    while (tab[i] != NULL && tab[i] != ID_TOMB)
        i = (i + 1) & (uint32_t)(cap - 1);
    tab[i] = n;
}

static void ids_grow(tui_ctx *ctx)
{
    int ncap = ctx->ids_cap ? ctx->ids_cap * 2 : 32;
    tui_node **nt = calloc((size_t)ncap, sizeof(*nt));
    if (nt == NULL)
        abort();
    for (int i = 0; i < ctx->ids_cap; i++)
        if (ctx->ids[i] && ctx->ids[i] != ID_TOMB)
            ids_insert_raw(nt, ncap, ctx->ids[i]);
    free(ctx->ids);
    ctx->ids = nt;
    ctx->ids_cap = ncap;
    ctx->ids_used = ctx->ids_count;
}

static void id_register(tui_node *n)
{
    tui_ctx *ctx = n->ctx;
    if (n->id_str == NULL || n->id_str[0] == '\0' ||
        (n->flags & NF_IN_ID_TABLE))
        return;
    if ((ctx->ids_used + 1) * 2 > ctx->ids_cap)
        ids_grow(ctx);
    ids_insert_raw(ctx->ids, ctx->ids_cap, n);
    ctx->ids_count++;
    ctx->ids_used++;
    n->flags |= NF_IN_ID_TABLE;
}

void tui__id_unregister(tui_node *n)
{
    tui_ctx *ctx = n->ctx;
    if (!(n->flags & NF_IN_ID_TABLE))
        return;
    uint32_t i = id_hash(n->id_str) & (uint32_t)(ctx->ids_cap - 1);
    while (ctx->ids[i] != NULL)
    {
        if (ctx->ids[i] == n)
        {
            ctx->ids[i] = ID_TOMB;
            ctx->ids_count--;
            break;
        }
        i = (i + 1) & (uint32_t)(ctx->ids_cap - 1);
    }
    n->flags &= ~NF_IN_ID_TABLE;
}

void tui__id_set(tui_node *n, const char *id)
{
    tui__id_unregister(n);
    free(n->id_str);
    n->id_str = id ? dup_n(id, strlen(id)) : NULL;
    n->a.id = n->id_str;
    id_register(n);
}

tui_node *tui_find(tui_ctx *ctx, const char *id)
{
    if (ctx == NULL || id == NULL || ctx->ids_cap == 0)
        return NULL;
    uint32_t i = id_hash(id) & (uint32_t)(ctx->ids_cap - 1);
    while (ctx->ids[i] != NULL)
    {
        tui_node *n = ctx->ids[i];
        if (n != ID_TOMB && strcmp(n->id_str, id) == 0)
            return n;
        i = (i + 1) & (uint32_t)(ctx->ids_cap - 1);
    }
    return NULL;
}

/* ------------------------------------------------------------------------ */
/* dirty tracking                                                           */
/* ------------------------------------------------------------------------ */

void tui__invalidate_caches(tui_node *n)
{
    n->flags &= ~NF_MC_VALID;
    n->hfw_n = 0;
    n->hfw_next = 0;
}

/* A node whose outer size cannot depend on its content or its parent. */
static bool is_boundary(const tui_node *n)
{
    return (n->flags & NF_IS_ROOT) ||
           (n->a.width.unit == TUI_UNIT_CELLS &&
            n->a.height.unit == TUI_UNIT_CELLS);
}

static void queue_root(tui_node *n)
{
    tui_ctx *ctx = n->ctx;
    if (n->flags & NF_QUEUED)
        return;
    if (ctx->nlroots == ctx->lroots_cap)
    {
        ctx->lroots_cap = ctx->lroots_cap ? ctx->lroots_cap * 2 : 16;
        ctx->lroots =
            realloc(ctx->lroots, (size_t)ctx->lroots_cap * sizeof(*ctx->lroots));
        if (ctx->lroots == NULL)
            abort();
    }
    ctx->lroots[ctx->nlroots++] = n;
    n->flags |= NF_QUEUED;
}

/* Drop cached intrinsic sizes from n up to the nearest boundary without
 * scheduling a relayout (used when a change provably keeps n's size). */
void tui__invalidate_chain(tui_node *n)
{
    for (tui_node *c = n; c != NULL; c = c->parent)
    {
        tui__invalidate_caches(c);
        if (is_boundary(c))
            return;
    }
}

void tui__mark_layout(tui_node *n)
{
    for (tui_node *c = n; c != NULL; c = c->parent)
    {
        bool already = (c->flags & NF_LAYOUT_DIRTY) != 0;
        tui__invalidate_caches(c);
        c->flags |= NF_LAYOUT_DIRTY;
        if (is_boundary(c))
        {
            queue_root(c);
            return;
        }
        if (already)
            return; /* everything above is already flagged */
    }
}

void tui__damage_add(tui_ctx *ctx, tui_rect r)
{
    r = tui__rect_intersect(r, (tui_rect){0, 0, ctx->width, ctx->height});
    if (tui__rect_empty(r))
        return;

    for (int i = 0; i < ctx->ndamage; i++)
        if (tui__rect_contains(ctx->damage[i], r))
            return;

    /* drop what the new rect swallows */
    int w = 0;
    for (int i = 0; i < ctx->ndamage; i++)
        if (!tui__rect_contains(r, ctx->damage[i]))
            ctx->damage[w++] = ctx->damage[i];
    ctx->ndamage = w;

    if (ctx->ndamage < TUI_MAX_DAMAGE)
    {
        ctx->damage[ctx->ndamage++] = r;
        return;
    }

    /* full: merge into the rect that grows the least */
    int best = 0;
    long best_cost = -1;
    for (int i = 0; i < ctx->ndamage; i++)
    {
        tui_rect u = tui__rect_union(ctx->damage[i], r);
        long cost = (long)u.w * u.h - (long)ctx->damage[i].w * ctx->damage[i].h;
        if (best_cost < 0 || cost < best_cost)
        {
            best_cost = cost;
            best = i;
        }
    }
    ctx->damage[best] = tui__rect_union(ctx->damage[best], r);
}

void tui__abs_origin(const tui_node *n, int *x, int *y)
{
    int ax = 0, ay = 0;
    for (const tui_node *c = n; c != NULL; c = c->parent)
    {
        ax += c->rect.x;
        ay += c->rect.y;
        if (c->parent)
        {
            ax -= c->parent->scroll_x;
            ay -= c->parent->scroll_y;
        }
    }
    *x = ax;
    *y = ay;
}

tui_rect tui__abs_rect(const tui_node *n)
{
    int x, y;
    tui__abs_origin(n, &x, &y);
    return (tui_rect){x, y, n->rect.w, n->rect.h};
}

bool tui__attached(const tui_node *n)
{
    const tui_node *c = n;
    while (c->parent)
        c = c->parent;
    return c == c->ctx->root;
}

bool tui_is_visible(const tui_node *n)
{
    if (!tui__attached(n))
        return false;
    for (const tui_node *c = n; c; c = c->parent)
        if (c->a.display == TUI_DISPLAY_NONE)
            return false;
    return true;
}

/* damage the node's current on-screen rect, if it is on screen */
void tui__damage_node(tui_node *n)
{
    if ((n->flags & NF_LAID_OUT) && tui_is_visible(n))
        tui__damage_add(n->ctx, tui__abs_rect(n));
}

/* ------------------------------------------------------------------------ */
/* context                                                                  */
/* ------------------------------------------------------------------------ */

tui_ctx *tui_create(int width, int height, const tui_options *opt)
{
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;

    tui_ctx *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
        return NULL;
    if (opt)
        ctx->opt = *opt;
    ctx->width = width;
    ctx->height = height;

    tui_attr ra = {0};
    ra.dir = TUI_COLUMN;
    ra.width = TUI_CELLS(width);
    ra.height = TUI_CELLS(height);
    ctx->root = tui_node_new(ctx, TUI_KIND_BOX, &ra);
    ctx->root->flags |= NF_IS_ROOT;
    ctx->root->rect = (tui_rect){0, 0, width, height};
    ctx->root->flags |= NF_LAID_OUT;
    tui__mark_layout(ctx->root);

    if (tui__screen_alloc(ctx, width, height) != 0)
    {
        tui_destroy(ctx);
        return NULL;
    }
    return ctx;
}

void tui_destroy(tui_ctx *ctx)
{
    if (ctx == NULL)
        return;
    if (ctx->root)
        tui__destroy_subtree(ctx->root);
    free(ctx->back);
    free(ctx->front);
    free(ctx->lroots);
    free(ctx->tickers);
    free(ctx->rqueue);
    free(ctx->ids);
    free(ctx->bstack);
    free(ctx->out);
    free(ctx->row_min);
    free(ctx->row_max);
    free(ctx);
}

tui_node *tui_root(tui_ctx *ctx)
{
    return ctx->root;
}

tui_size tui_screen_size(const tui_ctx *ctx)
{
    return (tui_size){ctx->width, ctx->height};
}

void tui_resize(tui_ctx *ctx, int width, int height)
{
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;
    if (width == ctx->width && height == ctx->height)
        return;
    ctx->width = width;
    ctx->height = height;
    ctx->ndamage = 0;
    tui__screen_alloc(ctx, width, height);

    tui_node *r = ctx->root;
    r->a.width = TUI_CELLS(width);
    r->a.height = TUI_CELLS(height);
    r->rect = (tui_rect){0, 0, width, height};
    tui__mark_layout(r);
}

/* ------------------------------------------------------------------------ */
/* node lifetime                                                            */
/* ------------------------------------------------------------------------ */

tui_node *tui_node_new(tui_ctx *ctx, tui_kind kind, const tui_attr *attr)
{
    tui_node *n = calloc(1, sizeof(*n));
    if (n == NULL)
        abort();
    n->ctx = ctx;
    n->kind = kind;
    n->flags = NF_LAYOUT_DIRTY;
    n->rs_w = n->rs_h = -1;

    if (attr)
        n->a = *attr;
    n->a.id = NULL;
    n->a.text = NULL;

    const char *text = attr && attr->text ? attr->text : "";
    n->text_len = strlen(text);
    n->text = dup_n(text, n->text_len);
    n->a.text = n->text;
    if (attr && attr->id)
        tui__id_set(n, attr->id);
    tui__tick_sync(n);
    return n;
}

static void queue_remove(tui_ctx *ctx, tui_node *n)
{
    for (int i = 0; i < ctx->nlroots; i++)
    {
        if (ctx->lroots[i] == n)
        {
            ctx->lroots[i] = ctx->lroots[--ctx->nlroots];
            break;
        }
    }
    n->flags &= ~NF_QUEUED;
}

void tui__destroy_subtree(tui_node *n)
{
    tui_node *c = n->first;
    while (c)
    {
        tui_node *next = c->next;
        tui__destroy_subtree(c);
        c = next;
    }
    if (n->a.on_destroy)
        n->a.on_destroy(n, n->a.userdata);
    if (n->flags & NF_QUEUED)
        queue_remove(n->ctx, n);
    if (n->flags & NF_RESIZE_Q)
        tui__resize_unqueue(n);
    tui__tick_unregister(n);
    tui__events_forget(n->ctx, n);
    tui__id_unregister(n);
    if (n->ctx->blast == n)
        n->ctx->blast = NULL;
    free(n->id_str);
    free(n->text);
    free(n);
}

void tui_node_free(tui_node *n)
{
    if (n == NULL)
        return;
    assert(!(n->flags & NF_IS_ROOT) && "the root cannot be freed");
    if (n->flags & NF_IS_ROOT)
        return;
    tui_detach(n);
    tui__destroy_subtree(n);
}

/* ------------------------------------------------------------------------ */
/* tree surgery                                                             */
/* ------------------------------------------------------------------------ */

void tui_detach(tui_node *n)
{
    tui_node *p = n->parent;
    if (p == NULL)
        return;

    tui__damage_node(n);

    if (n->prev)
        n->prev->next = n->next;
    else
        p->first = n->next;
    if (n->next)
        n->next->prev = n->prev;
    else
        p->last = n->prev;
    n->parent = n->prev = n->next = NULL;
    p->nchildren--;

    n->flags &= ~NF_LAID_OUT;
    n->flags |= NF_LAYOUT_DIRTY;
    tui__invalidate_caches(n);
    tui__mark_layout(p);
}

static void forget_layout(tui_node *n)
{
    n->flags &= ~NF_LAID_OUT;
    n->flags |= NF_LAYOUT_DIRTY;
    tui__invalidate_caches(n);
    for (tui_node *c = n->first; c; c = c->next)
        forget_layout(c);
}

static void link_child(tui_node *p, tui_node *c, tui_node *before)
{
    assert(c->ctx == p->ctx);
    assert(!(c->flags & NF_IS_ROOT));
    if (c->parent)
        tui_detach(c);

    /* a node cannot become its own ancestor */
    for (tui_node *a = p; a; a = a->parent)
    {
        assert(a != c && "cycle in the node tree");
        if (a == c)
            return;
    }

    c->parent = p;
    c->next = before;
    c->prev = before ? before->prev : p->last;
    if (c->prev)
        c->prev->next = c;
    else
        p->first = c;
    if (before)
        before->prev = c;
    else
        p->last = c;
    p->nchildren++;

    /* The subtree has no meaningful on-screen position in its new home, and
     * dirty fixed-size boundaries inside it must not be skipped: force a full
     * relayout of everything that is attached. */
    forget_layout(c);
    c->flags |= NF_LAYOUT_DIRTY;
    tui__mark_layout(p);
}

void tui_append(tui_node *p, tui_node *c)
{
    link_child(p, c, NULL);
}

void tui_prepend(tui_node *p, tui_node *c)
{
    link_child(p, c, p->first);
}

void tui_insert_before(tui_node *p, tui_node *c, tui_node *ref)
{
    assert(ref == NULL || ref->parent == p);
    if (ref == c)
        return;
    link_child(p, c, ref);
}

void tui_insert_at(tui_node *p, tui_node *c, int index)
{
    tui_node *ref = NULL;
    if (index >= 0)
    {
        ref = p->first;
        while (ref && index-- > 0)
            ref = ref->next;
        if (ref == c)
            ref = c->next;
    }
    link_child(p, c, ref);
}

void tui_clear_children(tui_node *n)
{
    while (n->first)
        tui_node_free(n->first);
}

/* ------------------------------------------------------------------------ */
/* navigation                                                               */
/* ------------------------------------------------------------------------ */

tui_node *tui_parent(const tui_node *n) { return n->parent; }
tui_node *tui_first_child(const tui_node *n) { return n->first; }
tui_node *tui_last_child(const tui_node *n) { return n->last; }
tui_node *tui_next_sibling(const tui_node *n) { return n->next; }
tui_node *tui_prev_sibling(const tui_node *n) { return n->prev; }
int tui_child_count(const tui_node *n) { return n->nchildren; }
tui_ctx *tui_node_ctx(const tui_node *n) { return n->ctx; }
tui_kind tui_node_kind(const tui_node *n) { return n->kind; }

tui_node *tui_child_at(const tui_node *n, int index)
{
    tui_node *c = n->first;
    while (c && index-- > 0)
        c = c->next;
    return c;
}

/* ------------------------------------------------------------------------ */
/* geometry queries                                                         */
/* ------------------------------------------------------------------------ */

tui_rect tui_get_rect(const tui_node *n)
{
    if (!(n->flags & NF_LAID_OUT))
        return (tui_rect){0};
    return tui__abs_rect(n);
}

tui_rect tui_get_content_rect(const tui_node *n)
{
    tui_rect r = tui_get_rect(n);
    tui_edges in = tui__insets(n);
    r.x += in.left;
    r.y += in.top;
    r.w = tui__max(0, r.w - in.left - in.right);
    r.h = tui__max(0, r.h - in.top - in.bottom);
    return r;
}
