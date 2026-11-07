#include "tui_internal.h"

/* ------------------------------------------------------------------------ */
/* on_update tick                                                           */
/* ------------------------------------------------------------------------ */

static void tick_remove(tui_node *n)
{
    tui_ctx *ctx = n->ctx;
    for (int i = 0; i < ctx->ntickers; i++)
    {
        if (ctx->tickers[i] != n)
            continue;
        if (ctx->ticking)
            ctx->tickers[i] = NULL; /* leave a hole, compacted afterwards */
        else
            ctx->tickers[i] = ctx->tickers[--ctx->ntickers];
        break;
    }
    n->flags &= ~NF_TICKING;
}

void tui__tick_unregister(tui_node *n)
{
    if (n->flags & NF_TICKING)
        tick_remove(n);
}

void tui__tick_sync(tui_node *n)
{
    tui_ctx *ctx = n->ctx;
    bool want = n->a.on_update != NULL;
    bool has = (n->flags & NF_TICKING) != 0;
    if (want && !has)
    {
        if (ctx->ntickers == ctx->tickers_cap)
        {
            ctx->tickers_cap = ctx->tickers_cap ? ctx->tickers_cap * 2 : 16;
            ctx->tickers = realloc(
                ctx->tickers, (size_t)ctx->tickers_cap * sizeof(*ctx->tickers));
            if (ctx->tickers == NULL)
                abort();
        }
        ctx->tickers[ctx->ntickers++] = n;
        n->flags |= NF_TICKING;
    }
    else if (!want && has)
        tick_remove(n);
}

void tui_update(tui_ctx *ctx, double dt)
{
    bool outer = ctx->ticking;
    ctx->ticking = true;
    int count = ctx->ntickers; /* tickers added by callbacks run next tick */
    for (int i = 0; i < count && i < ctx->ntickers; i++)
    {
        tui_node *n = ctx->tickers[i];
        if (n == NULL || !n->a.on_update || !tui_is_visible(n))
            continue;
        n->a.on_update(n, dt, n->a.userdata);
    }
    ctx->ticking = outer;
    if (!outer)
    {
        int w = 0;
        for (int i = 0; i < ctx->ntickers; i++)
            if (ctx->tickers[i])
                ctx->tickers[w++] = ctx->tickers[i];
        ctx->ntickers = w;
    }
}

/* ------------------------------------------------------------------------ */
/* on_resize queue                                                          */
/* ------------------------------------------------------------------------ */

void tui__resize_enqueue(tui_node *n)
{
    tui_ctx *ctx = n->ctx;
    if (n->flags & NF_RESIZE_Q)
        return;
    if (ctx->nrqueue == ctx->rqueue_cap)
    {
        ctx->rqueue_cap = ctx->rqueue_cap ? ctx->rqueue_cap * 2 : 16;
        ctx->rqueue = realloc(ctx->rqueue,
                              (size_t)ctx->rqueue_cap * sizeof(*ctx->rqueue));
        if (ctx->rqueue == NULL)
            abort();
    }
    ctx->rqueue[ctx->nrqueue++] = n;
    n->flags |= NF_RESIZE_Q;
}

void tui__resize_unqueue(tui_node *n)
{
    tui_ctx *ctx = n->ctx;
    for (int i = 0; i < ctx->nrqueue; i++)
    {
        if (ctx->rqueue[i] == n)
        {
            memmove(&ctx->rqueue[i], &ctx->rqueue[i + 1],
                    (size_t)(ctx->nrqueue - i - 1) * sizeof(*ctx->rqueue));
            ctx->nrqueue--;
            break;
        }
    }
    n->flags &= ~NF_RESIZE_Q;
}

bool tui__fire_resize(tui_ctx *ctx)
{
    bool fired = false;
    int budget = ctx->nrqueue;
    while (budget-- > 0 && ctx->nrqueue > 0)
    {
        tui_node *n = ctx->rqueue[0];
        tui__resize_unqueue(n);
        if (!n->a.on_resize || !tui__attached(n))
            continue;
        tui_edges in = tui__insets(n);
        int cw = tui__max(0, n->rect.w - in.left - in.right);
        int ch = tui__max(0, n->rect.h - in.top - in.bottom);
        if (cw == n->rs_w && ch == n->rs_h)
            continue;
        n->rs_w = cw;
        n->rs_h = ch;
        fired = true;
        n->a.on_resize(n, (tui_size){cw, ch}, n->a.userdata);
    }
    return fired;
}

/* ------------------------------------------------------------------------ */
/* scrolling + pseudo-class queries                                         */
/* ------------------------------------------------------------------------ */

void tui_scroll_into_view(tui_node *n)
{
    tui_node *c = n;
    for (tui_node *p = n->parent; p; c = p, p = p->parent)
    {
        if (!(p->flags & NF_LAID_OUT) || !(c->flags & NF_LAID_OUT))
            continue;
        if (p->content_w <= p->rect.w && p->content_h <= p->rect.h)
            continue; /* nothing to scroll here */
        tui_edges in = tui__insets(p);
        int sx = p->scroll_x, sy = p->scroll_y;

        int top = c->rect.y - in.top, bottom = c->rect.y + c->rect.h + in.bottom;
        if (top < sy)
            sy = top;
        else if (bottom > sy + p->rect.h)
            sy = bottom - p->rect.h;

        int left = c->rect.x - in.left, right = c->rect.x + c->rect.w + in.right;
        if (left < sx)
            sx = left;
        else if (right > sx + p->rect.w)
            sx = right - p->rect.w;

        tui_set_scroll(p, sx, sy);
    }
}

bool tui_is_hovered(const tui_node *n)
{
    for (const tui_node *c = n->ctx->hover; c; c = c->parent)
        if (c == n)
            return true;
    return false;
}

bool tui_is_focused(const tui_node *n)
{
    return n->ctx->focus == n;
}

bool tui_is_pressed(const tui_node *n)
{
    return n->ctx->pressed == n;
}
