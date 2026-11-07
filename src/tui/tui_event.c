#include "tui_internal.h"

void tui__events_forget(tui_ctx *ctx, tui_node *n)
{
    if (ctx->focus == n)
        ctx->focus = NULL;
    if (ctx->hover == n)
        ctx->hover = NULL;
    if (ctx->pressed == n)
        ctx->pressed = NULL;
    if (ctx->cur_owner == n)
        ctx->cur_owner = NULL;
    if (ctx->free_hook)
        ctx->free_hook(n, ctx->free_hook_ud);
}

void tui_set_key_hook(tui_ctx *ctx, tui_key_hook_fn fn, void *ud)
{
    ctx->key_hook = fn;
    ctx->key_hook_ud = ud;
}

void tui_set_node_free_hook(tui_ctx *ctx, tui_node_free_fn fn, void *ud)
{
    ctx->free_hook = fn;
    ctx->free_hook_ud = ud;
}

/* ---- terminal cursor ---------------------------------------------------- */

bool tui_hw_cursor_enabled(const tui_ctx *ctx)
{
    return !ctx->opt.soft_cursor;
}

void tui_set_cursor(tui_node *n, int x, int y, tui_cursor_shape shape)
{
    tui_ctx *ctx = n->ctx;
    ctx->cur_owner = n;
    ctx->cur_x = x;
    ctx->cur_y = y;
    ctx->cur_shape = shape;
}

void tui_hide_cursor(tui_node *n)
{
    if (n->ctx->cur_owner == n)
        n->ctx->cur_owner = NULL;
}

bool tui__cursor_resolve(tui_ctx *ctx, int *x, int *y)
{
    tui_node *n = ctx->cur_owner;
    if (ctx->opt.soft_cursor || n == NULL || !tui_is_visible(n))
        return false;
    tui_rect c = tui_get_content_rect(n);
    if (ctx->cur_x < 0 || ctx->cur_y < 0 || ctx->cur_x >= c.w ||
        ctx->cur_y >= c.h)
        return false;
    int sx = c.x + ctx->cur_x, sy = c.y + ctx->cur_y;
    if (sx < 0 || sy < 0 || sx >= ctx->width || sy >= ctx->height)
        return false;
    *x = sx;
    *y = sy;
    return true;
}

/* ---- hit testing -------------------------------------------------------- */

static tui_node *hit(tui_node *n, int ox, int oy, int x, int y)
{
    if (n->a.display == TUI_DISPLAY_NONE)
        return NULL;
    if (x < ox || y < oy || x >= ox + n->rect.w || y >= oy + n->rect.h)
        return NULL;

    int b = tui__border_w(n);
    bool in_pad = x >= ox + b && y >= oy + b && x < ox + n->rect.w - b &&
                  y < oy + n->rect.h - b;
    if (in_pad)
    {
        /* topmost first: absolute children (last painted wins), then flow */
        for (int pass = 0; pass < 2; pass++)
        {
            for (tui_node *c = n->last; c; c = c->prev)
            {
                bool abs = c->a.position == TUI_POS_ABSOLUTE;
                if (abs != (pass == 0))
                    continue;
                tui_node *r = hit(c, ox + c->rect.x - n->scroll_x,
                                  oy + c->rect.y - n->scroll_y, x, y);
                if (r)
                    return r;
            }
        }
    }
    return n;
}

tui_node *tui_hit_test(tui_ctx *ctx, int x, int y)
{
    return hit(ctx->root, 0, 0, x, y);
}

/* ---- delivery ----------------------------------------------------------- */

static bool deliver(tui_node *n, tui_event *ev)
{
    if (!n->a.on_event)
        return false;
    tui_rect r = tui__abs_rect(n);
    ev->current = n;
    ev->lx = ev->x - r.x;
    ev->ly = ev->y - r.y;
    return n->a.on_event(n, ev, n->a.userdata);
}

static bool bubble(tui_node *target, tui_event *ev)
{
    tui_ctx *ctx = target->ctx;
    ev->target = target;
    ctx->consumer = NULL;
    bool hooked = ev->type == TUI_EV_KEY && ctx->key_hook != NULL;
    for (tui_node *c = target; c; c = c->parent)
    {
        if (hooked && ctx->key_hook(c, ev, ctx->key_hook_ud))
        {
            ctx->consumer = c;
            return true;
        }
        if (deliver(c, ev))
        {
            ctx->consumer = c;
            return true;
        }
    }
    if (hooked && ctx->key_hook(NULL, ev, ctx->key_hook_ud))
        return true;
    return false;
}

bool tui_dispatch_from(tui_ctx *ctx, tui_node *start, tui_event *ev)
{
    if (start == NULL)
    {
        ev->target = NULL;
        return ev->type == TUI_EV_KEY && ctx->key_hook &&
               ctx->key_hook(NULL, ev, ctx->key_hook_ud);
    }
    return bubble(start, ev);
}

static bool is_self_or_ancestor(const tui_node *a, const tui_node *n)
{
    for (; n; n = n->parent)
        if (n == a)
            return true;
    return false;
}

static void notify(tui_node *n, tui_event_type type, tui_event *src)
{
    if (n == NULL || !n->a.on_event || !tui__attached(n))
        return;
    tui_event e = *src;
    e.type = type;
    e.target = n;
    deliver(n, &e);
}

/* ---- focus -------------------------------------------------------------- */

tui_node *tui_focused(const tui_ctx *ctx)
{
    return ctx->focus && tui__attached(ctx->focus) ? ctx->focus : NULL;
}

void tui_focus(tui_ctx *ctx, tui_node *n)
{
    if (ctx->focus == n)
        return;
    tui_event e = {0};
    tui_node *old = ctx->focus;
    ctx->focus = n;
    notify(old, TUI_EV_BLUR, &e);
    notify(n, TUI_EV_FOCUS, &e);
}

static int collect_focusable(tui_node *n, tui_node **out, int cap, int count)
{
    if (n->a.display == TUI_DISPLAY_NONE)
        return count;
    if (n->a.focusable)
    {
        if (out && count < cap)
            out[count] = n;
        count++;
    }
    for (tui_node *c = n->first; c; c = c->next)
        count = collect_focusable(c, out, cap, count);
    return count;
}

static void focus_step(tui_ctx *ctx, int dir)
{
    int n = collect_focusable(ctx->root, NULL, 0, 0);
    if (n == 0)
        return;
    tui_node **list = malloc((size_t)n * sizeof(*list));
    if (list == NULL)
        abort();
    collect_focusable(ctx->root, list, n, 0);
    int cur = -1;
    for (int i = 0; i < n; i++)
        if (list[i] == ctx->focus)
            cur = i;
    int next = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
    tui_node *target = list[next];
    free(list);
    tui_focus(ctx, target);
}

void tui_focus_next(tui_ctx *ctx) { focus_step(ctx, 1); }
void tui_focus_prev(tui_ctx *ctx) { focus_step(ctx, -1); }

/* ---- dispatch ----------------------------------------------------------- */

bool tui_dispatch(tui_ctx *ctx, tui_event *ev)
{
    switch (ev->type)
    {
    case TUI_EV_RESIZE:
        tui_resize(ctx, ev->width, ev->height);
        return true;

    case TUI_EV_TEXT: {
        tui_node *t = tui_focused(ctx);
        return bubble(t ? t : ctx->root, ev);
    }

    case TUI_EV_KEY: {
        tui_node *t = tui_focused(ctx);
        bool used = bubble(t ? t : ctx->root, ev);
        if (!used && ev->key == TUI_KEY_TAB)
        {
            if (ev->mod & TUI_MOD_SHIFT)
                tui_focus_prev(ctx);
            else
                tui_focus_next(ctx);
            used = true;
        }
        return used;
    }

    case TUI_EV_MOUSE_DOWN:
    case TUI_EV_MOUSE_UP:
    case TUI_EV_MOUSE_MOVE:
    case TUI_EV_SCROLL: {
        tui_node *t = tui_hit_test(ctx, ev->x, ev->y);

        if (ctx->hover != t)
        {
            /* like :hover / mouseenter: every node on the changed part of the
             * ancestor chain is notified, innermost first */
            tui_node *old = ctx->hover;
            ctx->hover = t;
            for (tui_node *c = old; c && !is_self_or_ancestor(c, t);
                 c = c->parent)
                notify(c, TUI_EV_MOUSE_LEAVE, ev);
            for (tui_node *c = t; c && !is_self_or_ancestor(c, old);
                 c = c->parent)
                notify(c, TUI_EV_MOUSE_ENTER, ev);
        }

        if (ev->type == TUI_EV_MOUSE_DOWN)
        {
            ctx->pressed = t;
            for (tui_node *c = t; c; c = c->parent)
            {
                if (c->a.focusable)
                {
                    tui_focus(ctx, c);
                    break;
                }
            }
        }
        else if (ev->type == TUI_EV_MOUSE_MOVE && ctx->pressed &&
                 tui__attached(ctx->pressed))
        {
            t = ctx->pressed; /* drag capture */
        }
        else if (ev->type == TUI_EV_MOUSE_UP && ctx->pressed)
        {
            /* the node that got the press also gets the release */
            tui_node *p = ctx->pressed;
            ctx->pressed = NULL;
            if (tui__attached(p))
                t = p;
        }
        if (!t)
            return false;
        bool used = bubble(t, ev);
        if (ev->type == TUI_EV_MOUSE_DOWN && ctx->consumer)
            ctx->pressed = ctx->consumer; /* drag/release go to the consumer */
        return used;
    }

    default:
        return false;
    }
}
