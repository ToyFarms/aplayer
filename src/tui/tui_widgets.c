#include "tui_widgets.h"

#include <stdlib.h>
#include <string.h>

/*
 * Structural widgets: hr / vr / spacer, button, checkbox, scroll.
 * Public API only (tui.h): this file is the reference for custom widgets.
 *
 * Pattern used by every widget:
 *   1. copy the caller's attr and fill in defaults,
 *   2. allocate a state struct, hang it on `userdata`,
 *   3. set on_event / paint / ... callbacks and on_destroy (frees the state),
 *   4. create the node(s) with tui__open()/tui__close() so they land under the
 *      builder cursor like any TUI_BOX would.
 */

/* ------------------------------------------------------------------------ */
/* theme                                                                    */
/* ------------------------------------------------------------------------ */

static tui_theme g_theme = {
    .fg = {220, 220, 220, 255},
    .fg_muted = {130, 130, 130, 255},
    .surface = {45, 45, 50, 255},
    .surface_hover = {66, 66, 78, 255},
    .surface_active = {92, 92, 112, 255},
    .accent = {230, 200, 150, 255},
    .accent_fg = {20, 20, 20, 255},
    .track = {50, 50, 56, 255},
    .border = {90, 90, 100, 255},
};

const tui_theme *tui_theme_get(void)
{
    return &g_theme;
}

void tui_theme_set(const tui_theme *theme)
{
    g_theme = *theme;
}

/* ------------------------------------------------------------------------ */
/* helpers                                                                  */
/* ------------------------------------------------------------------------ */

static tui_attr base_attr(const tui_attr *in)
{
    tui_attr a = {0};
    if (in)
        a = *in;
    return a;
}

static bool edges_zero(tui_edges e)
{
    return !e.top && !e.right && !e.bottom && !e.left;
}

static color_t mix(color_t a, color_t b, float t)
{
    return COLOR((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t),
                 (int)(a.b + (b.b - a.b) * t));
}

static void free_state(tui_node *n, void *ud)
{
    (void)n;
    free(ud);
}

/* create a childless node under the builder cursor */
static tui_node *make_leaf(tui_ctx *ctx, tui_kind kind, const tui_attr *a)
{
    tui_node *n = tui__open(ctx, kind, a);
    tui__close(ctx);
    return n;
}

/* ------------------------------------------------------------------------ */
/* hr / vr / spacer                                                         */
/* ------------------------------------------------------------------------ */

static void rule_paint(tui_node *n, tui_canvas *c, void *vertical)
{
    tui_attr a = tui_get_attr(n);
    tui_pen pen = {a.fg.a ? a.fg : g_theme.border, COLOR_NONE, a.attrs};
    int w = tui_canvas_width(c), h = tui_canvas_height(c);
    if (vertical)
        tui_canvas_vline(c, (w - 1) / 2, 0, h, 0x2502, pen);
    else
        tui_canvas_hline(c, 0, (h - 1) / 2, w, 0x2500, pen);
}

tui_node *tui_hr(tui_ctx *ctx, const tui_attr *attr)
{
    tui_attr a = base_attr(attr);
    if (a.height.unit == TUI_UNIT_AUTO)
        a.height = TUI_CELLS(1);
    a.paint = rule_paint;
    a.userdata = NULL;
    return make_leaf(ctx, TUI_KIND_BOX, &a);
}

tui_node *tui_vr(tui_ctx *ctx, const tui_attr *attr)
{
    tui_attr a = base_attr(attr);
    if (a.width.unit == TUI_UNIT_AUTO)
        a.width = TUI_CELLS(1);
    a.paint = rule_paint;
    a.userdata = (void *)1; /* non-NULL selects the vertical variant */
    return make_leaf(ctx, TUI_KIND_BOX, &a);
}

tui_node *tui_spacer(tui_ctx *ctx, const tui_attr *attr)
{
    tui_attr a = base_attr(attr);
    if (a.grow == 0)
        a.grow = 1;
    return make_leaf(ctx, TUI_KIND_BOX, &a);
}

/* ------------------------------------------------------------------------ */
/* button                                                                   */
/* ------------------------------------------------------------------------ */

typedef struct button_state
{
    tui_click_fn cb;
    void *ud;
    color_t base_bg, base_fg;
    color_t hover_bg, hover_fg; /* a == 0: derive automatically */
    bool down;
} button_state;

static void button_refresh(tui_node *n, button_state *b)
{
    bool hovered = tui_is_hovered(n), focused = tui_is_focused(n);
    color_t bg = b->base_bg.a ? b->base_bg : g_theme.surface;
    if (b->down)
        bg = b->base_bg.a ? mix(bg, COLOR(255, 255, 255), 0.3f)
                          : g_theme.surface_active;
    else if (hovered)
        bg = b->base_bg.a ? mix(bg, COLOR(255, 255, 255), 0.15f)
                          : g_theme.surface_hover;

    if ((hovered || b->down) && b->hover_bg.a)
        bg = b->hover_bg;

    tui_attr a = tui_get_attr(n);
    a.bg = bg;
    a.fg = focused ? g_theme.accent : (b->base_fg.a ? b->base_fg : g_theme.fg);
    if ((hovered || b->down) && b->hover_fg.a)
        a.fg = b->hover_fg;
    if (focused)
        a.attrs |= TUI_BOLD;
    else
        a.attrs &= (uint16_t)~TUI_BOLD;
    tui_set_attr(n, &a);
}

static bool button_event(tui_node *n, tui_event *ev, void *ud)
{
    button_state *b = ud;
    switch (ev->type)
    {
    case TUI_EV_MOUSE_ENTER:
    case TUI_EV_MOUSE_LEAVE:
    case TUI_EV_FOCUS:
    case TUI_EV_BLUR:
        button_refresh(n, b);
        return false;
    case TUI_EV_MOUSE_DOWN:
        if (ev->button != 0)
            return false;
        b->down = true;
        button_refresh(n, b);
        return true;
    case TUI_EV_MOUSE_UP: {
        bool was = b->down;
        b->down = false;
        button_refresh(n, b);
        tui_rect r = tui_get_rect(n);
        bool inside =
            ev->lx >= 0 && ev->ly >= 0 && ev->lx < r.w && ev->ly < r.h;
        if (was && inside && b->cb)
            b->cb(n, b->ud); /* may free the button: touch nothing after */
        return true;
    }
    case TUI_EV_KEY:
        if (ev->key == TUI_KEY_ENTER || ev->key == ' ')
        {
            if (b->cb)
                b->cb(n, b->ud);
            return true;
        }
        return false;
    default:
        return false;
    }
}

tui_node *tui_button(tui_ctx *ctx, const char *label, tui_click_fn on_click,
                     void *ud, const tui_attr *attr)
{
    tui_attr a = base_attr(attr);
    button_state *b = calloc(1, sizeof(*b));
    if (b == NULL)
        abort();
    b->cb = on_click;
    b->ud = ud;
    b->base_bg = a.bg;
    b->base_fg = a.fg;

    if (edges_zero(a.padding))
        a.padding = (tui_edges){0, 1, 0, 1};
    a.dir = TUI_ROW;
    if (a.justify == TUI_JUSTIFY_START)
        a.justify = TUI_JUSTIFY_CENTER;
    if (a.align_items == TUI_ALIGN_AUTO)
        a.align_items = TUI_ALIGN_CENTER;
    a.focusable = true;
    a.bg = b->base_bg.a ? b->base_bg : g_theme.surface;
    a.fg = b->base_fg.a ? b->base_fg : g_theme.fg;
    a.on_event = button_event;
    a.on_destroy = free_state;
    a.userdata = b;

    tui_node *n = tui__open(ctx, TUI_KIND_BOX, &a);
    tui__open(ctx, TUI_KIND_TEXT,
              &(tui_attr){._init = 0, .text = label ? label : ""});
    tui__close(ctx);
    tui__close(ctx);
    return n;
}

void tui_button_set_style(tui_node *button, color_t bg, color_t fg,
                          color_t hover_bg, color_t hover_fg)
{
    tui_attr a = tui_get_attr(button);
    if (a.on_event != button_event)
        return;
    button_state *b = a.userdata;
    b->base_bg = bg;
    b->base_fg = fg;
    b->hover_bg = hover_bg;
    b->hover_fg = hover_fg;
    button_refresh(button, b);
}

void tui_button_set_label(tui_node *button, const char *label)
{
    tui_node *t = tui_first_child(button);
    if (t)
        tui_set_text(t, label);
}

/* ------------------------------------------------------------------------ */
/* checkbox                                                                 */
/* ------------------------------------------------------------------------ */

typedef struct checkbox_state
{
    bool checked, down;
    tui_toggle_fn cb;
    void *ud;
    tui_node *glyph;
} checkbox_state;

static checkbox_state *checkbox_of(const tui_node *n)
{
    tui_attr a = tui_get_attr(n);
    return a.on_destroy == free_state ? a.userdata : NULL;
}

static void checkbox_refresh(tui_node *n, checkbox_state *c)
{
    tui_set_text(c->glyph, c->checked ? "[x]" : "[ ]");
    tui_set_fg(c->glyph, c->checked ? g_theme.accent : g_theme.fg_muted);

    tui_attr a = tui_get_attr(n);
    bool lit = tui_is_focused(n) || tui_is_hovered(n);
    if (lit)
        a.attrs |= TUI_BOLD;
    else
        a.attrs &= (uint16_t)~TUI_BOLD;
    tui_set_attr(n, &a);
}

static void checkbox_toggle(tui_node *n, checkbox_state *c)
{
    c->checked = !c->checked;
    checkbox_refresh(n, c);
    if (c->cb)
        c->cb(n, c->checked, c->ud);
}

static bool checkbox_event(tui_node *n, tui_event *ev, void *ud)
{
    checkbox_state *c = ud;
    switch (ev->type)
    {
    case TUI_EV_MOUSE_ENTER:
    case TUI_EV_MOUSE_LEAVE:
    case TUI_EV_FOCUS:
    case TUI_EV_BLUR:
        checkbox_refresh(n, c);
        return false;
    case TUI_EV_MOUSE_DOWN:
        if (ev->button != 0)
            return false;
        c->down = true;
        return true;
    case TUI_EV_MOUSE_UP: {
        bool was = c->down;
        c->down = false;
        tui_rect r = tui_get_rect(n);
        if (was && ev->lx >= 0 && ev->ly >= 0 && ev->lx < r.w && ev->ly < r.h)
            checkbox_toggle(n, c);
        return true;
    }
    case TUI_EV_KEY:
        if (ev->key == ' ' || ev->key == TUI_KEY_ENTER)
        {
            checkbox_toggle(n, c);
            return true;
        }
        return false;
    default:
        return false;
    }
}

tui_node *tui_checkbox(tui_ctx *ctx, const char *label, bool checked,
                       tui_toggle_fn on_change, void *ud, const tui_attr *attr)
{
    tui_attr a = base_attr(attr);
    checkbox_state *c = calloc(1, sizeof(*c));
    if (c == NULL)
        abort();
    c->checked = checked;
    c->cb = on_change;
    c->ud = ud;

    a.dir = TUI_ROW;
    if (a.gap == 0)
        a.gap = 1;
    if (a.align_items == TUI_ALIGN_AUTO)
        a.align_items = TUI_ALIGN_CENTER;
    a.focusable = true;
    a.on_event = checkbox_event;
    a.on_destroy = free_state;
    a.userdata = c;

    tui_node *n = tui__open(ctx, TUI_KIND_BOX, &a);
    c->glyph = tui__open(ctx, TUI_KIND_TEXT,
                         &(tui_attr){._init = 0, .text = "[ ]",
                                     .width = TUI_CELLS(3),
                                     .shrink = TUI_NO_SHRINK});
    tui__close(ctx);
    tui__open(ctx, TUI_KIND_TEXT,
              &(tui_attr){._init = 0, .text = label ? label : "",
                          .text_wrap = TUI_TEXT_ELLIPSIS});
    tui__close(ctx);
    tui__close(ctx);
    checkbox_refresh(n, c);
    return n;
}

bool tui_checkbox_checked(const tui_node *cb)
{
    checkbox_state *c = checkbox_of(cb);
    return c ? c->checked : false;
}

void tui_checkbox_set(tui_node *cb, bool checked)
{
    checkbox_state *c = checkbox_of(cb);
    if (c && c->checked != checked)
    {
        c->checked = checked;
        checkbox_refresh(cb, c);
    }
}

/* ------------------------------------------------------------------------ */
/* scroll container                                                         */
/* ------------------------------------------------------------------------ */

typedef struct scroll_state
{
    bool dragging;
} scroll_state;

static int scroll_max_y(tui_node *n)
{
    tui_rect r = tui_get_rect(n);
    int b = 0;
    tui_attr a = tui_get_attr(n);
    if (a.border != TUI_BORDER_NONE)
        b = 1;
    int viewport = r.h - 2 * b;
    int m = tui_get_content_size(n).h - viewport;
    return m > 0 ? m : 0;
}

static void scroll_to_ratio(tui_node *n, float ratio)
{
    tui_size s = tui_get_scroll(n);
    if (ratio < 0)
        ratio = 0;
    if (ratio > 1)
        ratio = 1;
    tui_set_scroll(n, s.w, (int)(ratio * (float)scroll_max_y(n) + 0.5f));
}

static bool on_scrollbar(tui_node *n, tui_event *ev)
{
    tui_rect r = tui_get_rect(n);
    tui_attr a = tui_get_attr(n);
    int b = a.border != TUI_BORDER_NONE ? 1 : 0;
    return scroll_max_y(n) > 0 && ev->lx >= r.w - 1 - b && ev->lx < r.w - b;
}

static bool scroll_event(tui_node *n, tui_event *ev, void *ud)
{
    scroll_state *st = ud;
    tui_size before = tui_get_scroll(n);
    tui_rect r = tui_get_rect(n);
    int page = r.h > 1 ? r.h - 1 : 1;

    switch (ev->type)
    {
    case TUI_EV_SCROLL:
        if (ev->mod & TUI_MOD_SHIFT)
            tui_scroll_by(n, ev->scroll_delta * 3, 0);
        else
            tui_scroll_by(n, 0, ev->scroll_delta * 3);
        break;
    case TUI_EV_KEY:
        switch (ev->key)
        {
        case TUI_KEY_UP:       tui_scroll_by(n, 0, -1); break;
        case TUI_KEY_DOWN:     tui_scroll_by(n, 0, 1); break;
        case TUI_KEY_PAGEUP:   tui_scroll_by(n, 0, -page); break;
        case TUI_KEY_PAGEDOWN: tui_scroll_by(n, 0, page); break;
        case TUI_KEY_HOME:     tui_set_scroll(n, before.w, 0); break;
        case TUI_KEY_END:      tui_set_scroll(n, before.w, 1 << 29); break;
        default:
            return false;
        }
        break;
    case TUI_EV_MOUSE_DOWN:
        if (ev->button == 0 && ev->target == n && on_scrollbar(n, ev))
        {
            st->dragging = true;
            scroll_to_ratio(n, (float)ev->ly / (float)(r.h > 1 ? r.h - 1 : 1));
            return true;
        }
        return false;
    case TUI_EV_MOUSE_MOVE:
        if (!st->dragging)
            return false;
        scroll_to_ratio(n, (float)ev->ly / (float)(r.h > 1 ? r.h - 1 : 1));
        return true;
    case TUI_EV_MOUSE_UP:
        if (st->dragging)
        {
            st->dragging = false;
            return true;
        }
        return false;
    case TUI_EV_FOCUS:
    case TUI_EV_BLUR:
    case TUI_EV_MOUSE_ENTER:
    case TUI_EV_MOUSE_LEAVE:
        tui_invalidate(n); /* scrollbar highlight */
        return false;
    default:
        return false;
    }

    tui_size after = tui_get_scroll(n);
    return after.w != before.w || after.h != before.h; /* else bubble up */
}

static void scroll_paint_over(tui_node *n, tui_canvas *c, void *ud)
{
    (void)ud;
    int vh = tui_canvas_height(c), cw = tui_canvas_width(c);
    int total = tui_get_content_size(n).h;
    if (vh <= 0 || cw <= 0 || total <= vh)
        return;

    int max = total - vh;
    int thumb = vh * vh / total;
    if (thumb < 1)
        thumb = 1;
    int sy = tui_get_scroll(n).h;
    int top = max > 0 ? (sy * (vh - thumb) + max / 2) / max : 0;

    bool lit = tui_is_focused(n) || tui_is_hovered(n);
    tui_pen track = {g_theme.border, COLOR_NONE, 0};
    tui_pen bar = {lit ? g_theme.accent : g_theme.fg_muted, COLOR_NONE, 0};
    for (int y = 0; y < vh; y++)
    {
        bool in = y >= top && y < top + thumb;
        tui_canvas_put(c, cw - 1, y, in ? 0x2503 : 0x2502, in ? bar : track);
    }
}

tui_node *tui_scroll_begin(tui_ctx *ctx, const tui_attr *attr)
{
    tui_attr a = base_attr(attr);
    scroll_state *st = calloc(1, sizeof(*st));
    if (st == NULL)
        abort();

    if (a.dir == TUI_ROW)
        a.dir = TUI_COLUMN; /* a scroll area is a vertical list by default */
    if (edges_zero(a.padding))
        a.padding.right = 1; /* room for the scrollbar */
    a.scroll = true;
    a.focusable = true;
    a.on_event = scroll_event;
    a.paint_over = scroll_paint_over;
    a.on_destroy = free_state;
    a.userdata = st;
    return tui__open(ctx, TUI_KIND_BOX, &a);
}
