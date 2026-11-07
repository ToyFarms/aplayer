#include "tui_widgets.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
 * Data widgets: single-line text input, progress bar, slider.
 * Public API only (tui.h) - same rules as tui_widgets.c.
 */

static bool edges_zero(tui_edges e)
{
    return !e.top && !e.right && !e.bottom && !e.left;
}

static color_t mix(color_t a, color_t b, float t)
{
    return COLOR((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t),
                 (int)(a.b + (b.b - a.b) * t));
}

static float clamp01(float v)
{
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

/* ------------------------------------------------------------------------ */
/* text input                                                               */
/* ------------------------------------------------------------------------ */

typedef struct input_state
{
    char *buf;
    size_t len, cap;
    size_t cursor; /* byte offset, always on a codepoint boundary */
    int scroll;    /* first visible column */
    char *placeholder;
    bool password;
    color_t base_bg;
    tui_text_fn on_change, on_submit;
    void *change_ud, *submit_ud;
} input_state;

static void input_free(tui_node *n, void *ud)
{
    (void)n;
    input_state *st = ud;
    free(st->buf);
    free(st->placeholder);
    free(st);
}

static input_state *input_of(const tui_node *n)
{
    tui_attr a = tui_get_attr(n);
    return a.on_destroy == input_free ? a.userdata : NULL;
}

static void input_reserve(input_state *st, size_t need)
{
    if (need + 1 <= st->cap)
        return;
    size_t cap = st->cap ? st->cap : 32;
    while (cap < need + 1)
        cap *= 2;
    char *nb = realloc(st->buf, cap);
    if (nb == NULL)
        abort();
    st->buf = nb;
    st->cap = cap;
}

static size_t prev_boundary(const input_state *st, size_t i)
{
    if (i == 0)
        return 0;
    i--;
    while (i > 0 && ((unsigned char)st->buf[i] & 0xC0) == 0x80)
        i--;
    return i;
}

static size_t next_boundary(const input_state *st, size_t i)
{
    if (i >= st->len)
        return st->len;
    uint32_t cp;
    size_t k = tui_utf8_decode(st->buf + i, st->len - i, &cp);
    return i + (k ? k : 1);
}

static int cell_w(const input_state *st, uint32_t cp)
{
    return st->password ? 1 : tui_cp_width(cp);
}

/* display column of byte offset `upto` */
static int col_of(const input_state *st, size_t upto)
{
    int col = 0;
    size_t i = 0;
    while (i < upto && i < st->len)
    {
        uint32_t cp;
        i += tui_utf8_decode(st->buf + i, st->len - i, &cp);
        col += cell_w(st, cp);
    }
    return col;
}

/* byte offset whose cell contains display column `col` */
static size_t offset_at_col(const input_state *st, int col)
{
    int c = 0;
    size_t i = 0;
    while (i < st->len)
    {
        uint32_t cp;
        size_t k = tui_utf8_decode(st->buf + i, st->len - i, &cp);
        int w = cell_w(st, cp);
        if (col < c + (w > 0 ? w : 1))
            return i;
        c += w;
        i += k ? k : 1;
    }
    return st->len;
}

/* keep the cursor inside the window of `width` columns */
static void input_fix_scroll(input_state *st, int width)
{
    if (width < 1)
        return;
    int cc = col_of(st, st->cursor);
    if (cc < st->scroll)
        st->scroll = cc;
    if (cc >= st->scroll + width)
        st->scroll = cc - width + 1;
    if (st->scroll < 0)
        st->scroll = 0;
}

/* Terminal cursor on the caret: this is where an IME draws its pre-edit text
 * and candidate window. */
static void input_cursor(tui_node *n, input_state *st)
{
    if (tui_is_focused(n) && tui_hw_cursor_enabled(tui_node_ctx(n)))
        tui_set_cursor(n, col_of(st, st->cursor) - st->scroll, 0,
                       TUI_CURSOR_BAR);
    else
        tui_hide_cursor(n);
}

static void input_changed(tui_node *n, input_state *st)
{
    tui_rect c = tui_get_content_rect(n);
    input_fix_scroll(st, c.w);
    input_cursor(n, st);
    tui_invalidate(n);
    if (st->on_change)
        st->on_change(n, st->buf, st->change_ud);
}

static void input_insert(tui_node *n, input_state *st, uint32_t cp)
{
    char enc[4];
    size_t k = tui_utf8_encode(cp, enc);
    input_reserve(st, st->len + k);
    memmove(st->buf + st->cursor + k, st->buf + st->cursor, st->len - st->cursor);
    memcpy(st->buf + st->cursor, enc, k);
    st->len += k;
    st->cursor += k;
    st->buf[st->len] = '\0';
    input_changed(n, st);
}

/* Inserts a whole string at the caret with ONE change notification (IME
 * commits, paste). Control characters are dropped, line breaks become blanks. */
static void input_insert_text(tui_node *n, input_state *st, const char *s,
                              size_t len)
{
    input_reserve(st, st->len + len);
    size_t i = 0, w = 0;
    char *tmp = malloc(len + 1);
    if (tmp == NULL)
        abort();
    while (i < len)
    {
        uint32_t cp;
        size_t k = tui_utf8_decode(s + i, len - i, &cp);
        if (k == 0)
            break;
        if (cp == '\n' || cp == '\r' || cp == '\t')
            cp = ' ';
        if (cp >= 0x20 && cp != 0x7F && !(cp >= 0x80 && cp < 0xA0))
            w += tui_utf8_encode(cp, tmp + w);
        i += k;
    }
    if (w > 0)
    {
        input_reserve(st, st->len + w);
        memmove(st->buf + st->cursor + w, st->buf + st->cursor, st->len - st->cursor);
        memcpy(st->buf + st->cursor, tmp, w);
        st->len += w;
        st->cursor += w;
        st->buf[st->len] = '\0';
        input_changed(n, st);
    }
    free(tmp);
}

static void input_erase(tui_node *n, input_state *st, size_t from, size_t to)
{
    if (from >= to)
        return;
    memmove(st->buf + from, st->buf + to, st->len - to);
    st->len -= to - from;
    st->buf[st->len] = '\0';
    st->cursor = from;
    input_changed(n, st);
}

static size_t word_left(const input_state *st, size_t i)
{
    while (i > 0 && st->buf[prev_boundary(st, i)] == ' ')
        i = prev_boundary(st, i);
    while (i > 0 && st->buf[prev_boundary(st, i)] != ' ')
        i = prev_boundary(st, i);
    return i;
}

static size_t word_right(const input_state *st, size_t i)
{
    while (i < st->len && st->buf[i] == ' ')
        i = next_boundary(st, i);
    while (i < st->len && st->buf[i] != ' ')
        i = next_boundary(st, i);
    return i;
}

static void input_move(tui_node *n, input_state *st, size_t to)
{
    if (to == st->cursor)
        return;
    st->cursor = to;
    tui_rect c = tui_get_content_rect(n);
    input_fix_scroll(st, c.w);
    input_cursor(n, st);
    tui_invalidate(n);
}

static void input_refresh(tui_node *n, input_state *st)
{
    const tui_theme *t = tui_theme_get();
    color_t bg = st->base_bg.a ? st->base_bg : t->surface;
    if (tui_is_focused(n))
        bg = st->base_bg.a ? mix(bg, COLOR(255, 255, 255), 0.12f)
                           : t->surface_hover;
    tui_set_bg(n, bg);
    input_cursor(n, st);
    tui_invalidate(n); /* cursor visibility */
}

static bool input_event(tui_node *n, tui_event *ev, void *ud)
{
    input_state *st = ud;
    switch (ev->type)
    {
    case TUI_EV_FOCUS:
    case TUI_EV_BLUR:
        input_refresh(n, st);
        return false;
    case TUI_EV_MOUSE_DOWN: {
        if (ev->button != 0)
            return false;
        tui_rect r = tui_get_rect(n), c = tui_get_content_rect(n);
        int col = st->scroll + (ev->lx - (c.x - r.x));
        input_move(n, st, offset_at_col(st, col < 0 ? 0 : col));
        return true;
    }
    case TUI_EV_MOUSE_UP:
        return true;
    case TUI_EV_TEXT:
        input_insert_text(n, st, ev->text, ev->text_len);
        return true;
    case TUI_EV_KEY:
        break;
    default:
        return false;
    }

    uint32_t key = ev->key;
    bool ctrl = ev->mod & TUI_MOD_CTRL, alt = ev->mod & TUI_MOD_ALT;

    if (ctrl && key < 0x110000)
    {
        switch (tolower((int)key))
        {
        case 'a': input_move(n, st, 0); return true;
        case 'e': input_move(n, st, st->len); return true;
        case 'u': input_erase(n, st, 0, st->cursor); return true;
        case 'k': input_erase(n, st, st->cursor, st->len); return true;
        case 'w': input_erase(n, st, word_left(st, st->cursor), st->cursor); return true;
        case 'd': input_erase(n, st, st->cursor, next_boundary(st, st->cursor)); return true;
        default:  return false;
        }
    }

    switch (key)
    {
    case TUI_KEY_ENTER:
        if (st->on_submit)
            st->on_submit(n, st->buf, st->submit_ud);
        return true;
    case TUI_KEY_BACKSPACE:
        input_erase(n, st, prev_boundary(st, st->cursor), st->cursor);
        return true;
    case TUI_KEY_DELETE:
        input_erase(n, st, st->cursor, next_boundary(st, st->cursor));
        return true;
    case TUI_KEY_LEFT:
        input_move(n, st, (ctrl || alt) ? word_left(st, st->cursor)
                                    : prev_boundary(st, st->cursor));
        return true;
    case TUI_KEY_RIGHT:
        input_move(n, st, (ctrl || alt) ? word_right(st, st->cursor)
                                    : next_boundary(st, st->cursor));
        return true;
    case TUI_KEY_HOME: input_move(n, st, 0); return true;
    case TUI_KEY_END:  input_move(n, st, st->len); return true;
    default:
        break;
    }

    if (!alt && key >= 0x20 && key < 0x110000 && key != 0x7F)
    {
        input_insert(n, st, key);
        return true;
    }
    return false; /* Tab, Esc, arrows up/down...: let the app handle them */
}

static void input_paint(tui_node *n, tui_canvas *c, void *ud)
{
    input_state *st = ud;
    const tui_theme *t = tui_theme_get();
    tui_attr a = tui_get_attr(n);
    int W = tui_canvas_width(c);
    if (W <= 0)
        return;

    input_fix_scroll(st, W);
    input_cursor(n, st);
    bool soft = !tui_hw_cursor_enabled(tui_node_ctx(n));
    bool focused = tui_is_focused(n);
    tui_pen pen = {a.fg.a ? a.fg : t->fg, COLOR_NONE, a.attrs};
    tui_pen cur = pen;
    cur.attrs |= TUI_REVERSE;

    if (st->len == 0)
    {
        if (st->placeholder)
        {
            tui_pen dim = {t->fg_muted, COLOR_NONE, 0};
            tui_canvas_text(c, (focused && soft) ? 1 : 0, 0, st->placeholder, dim);
        }
        if (focused && soft)
            tui_canvas_put(c, 0, 0, ' ', cur);
        return;
    }

    int col = 0;
    size_t i = 0;
    int cursor_col = col_of(st, st->cursor);
    while (i < st->len)
    {
        uint32_t cp;
        i += tui_utf8_decode(st->buf + i, st->len - i, &cp);
        int w = cell_w(st, cp);
        if (w > 0 && col >= st->scroll)
        {
            int x = col - st->scroll;
            if (x + w > W)
                break;
            uint32_t shown = st->password ? 0x2022 : cp;
            tui_canvas_put(c, x, 0, shown,
                           (focused && soft && col == cursor_col) ? cur : pen);
        }
        col += w;
    }
    if (focused && soft && st->cursor == st->len && cursor_col - st->scroll < W)
        tui_canvas_put(c, cursor_col - st->scroll, 0, ' ', cur);
}

tui_node *tui_input(tui_ctx *ctx, const char *initial, const tui_attr *attr)
{
    const tui_theme *t = tui_theme_get();
    tui_attr a = {0};
    if (attr)
        a = *attr;

    input_state *st = calloc(1, sizeof(*st));
    if (st == NULL)
        abort();
    input_reserve(st, initial ? strlen(initial) : 0);
    if (initial)
    {
        st->len = strlen(initial);
        memcpy(st->buf, initial, st->len);
    }
    st->buf[st->len] = '\0';
    st->cursor = st->len;
    st->base_bg = a.bg;

    if (a.height.unit == TUI_UNIT_AUTO)
        a.height = TUI_CELLS(1);
    if (a.width.unit == TUI_UNIT_AUTO && a.grow == 0)
        a.grow = 1;
    if (edges_zero(a.padding))
        a.padding = (tui_edges){0, 1, 0, 1};
    a.bg = st->base_bg.a ? st->base_bg : t->surface;
    a.focusable = true;
    a.paint = input_paint;
    a.on_event = input_event;
    a.on_destroy = input_free;
    a.userdata = st;

    tui_node *n = tui__open(ctx, TUI_KIND_BOX, &a);
    tui__close(ctx);
    return n;
}

void tui_input_on_change(tui_node *n, tui_text_fn fn, void *ud)
{
    input_state *st = input_of(n);
    if (st)
    {
        st->on_change = fn;
        st->change_ud = ud;
    }
}

void tui_input_on_submit(tui_node *n, tui_text_fn fn, void *ud)
{
    input_state *st = input_of(n);
    if (st)
    {
        st->on_submit = fn;
        st->submit_ud = ud;
    }
}

const char *tui_input_text(const tui_node *n)
{
    input_state *st = input_of(n);
    return st ? st->buf : "";
}

void tui_input_set_text(tui_node *n, const char *utf8)
{
    input_state *st = input_of(n);
    if (!st)
        return;
    if (utf8 == NULL)
        utf8 = "";
    size_t len = strlen(utf8);
    if (len == st->len && memcmp(utf8, st->buf, len) == 0)
        return;
    input_reserve(st, len);
    memmove(st->buf, utf8, len + 1); /* tolerate aliasing with our buffer */
    st->len = len;
    st->cursor = len;
    st->scroll = 0;
    tui_rect c = tui_get_content_rect(n);
    input_fix_scroll(st, c.w);
    input_cursor(n, st);
    tui_invalidate(n);
}

void tui_input_set_placeholder(tui_node *n, const char *utf8)
{
    input_state *st = input_of(n);
    if (!st)
        return;
    free(st->placeholder);
    st->placeholder = NULL;
    if (utf8)
    {
        size_t len = strlen(utf8);
        st->placeholder = malloc(len + 1);
        if (st->placeholder == NULL)
            abort();
        memcpy(st->placeholder, utf8, len + 1);
    }
    tui_invalidate(n);
}

void tui_input_set_password(tui_node *n, bool password)
{
    input_state *st = input_of(n);
    if (!st || st->password == password)
        return;
    st->password = password;
    tui_rect c = tui_get_content_rect(n);
    input_fix_scroll(st, c.w);
    tui_invalidate(n);
}

/* ------------------------------------------------------------------------ */
/* progress / slider                                                        */
/* ------------------------------------------------------------------------ */

typedef struct bar_state
{
    float value;
    bool interactive, dragging;
    color_t fill, track; /* a == 0: theme */
    tui_value_fn cb;
    void *ud;
} bar_state;

static void bar_free(tui_node *n, void *ud)
{
    (void)n;
    free(ud);
}

static bar_state *bar_of(const tui_node *n)
{
    tui_attr a = tui_get_attr(n);
    return a.on_destroy == bar_free ? a.userdata : NULL;
}

static int eighths(float v, int w)
{
    return (int)(clamp01(v) * (float)w * 8.0f + 0.5f);
}

static void bar_paint(tui_node *n, tui_canvas *c, void *ud)
{
    bar_state *st = ud;
    const tui_theme *t = tui_theme_get();
    int W = tui_canvas_width(c), H = tui_canvas_height(c);
    if (W <= 0 || H <= 0)
        return;

    color_t fill = st->fill.a ? st->fill : t->accent;
    color_t trk = st->track.a ? st->track : t->track;
    if (st->interactive && !st->fill.a && !tui_is_focused(n))
        fill = mix(t->accent, t->track, 0.3f);

    int e = eighths(st->value, W);
    int full = e / 8, part = e % 8;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (x < full)
                tui_canvas_put(c, x, y, 0x2588, (tui_pen){fill, trk, 0});
            else if (x == full && part > 0)
                tui_canvas_put(c, x, y, 0x2590 - (uint32_t)part,
                               (tui_pen){fill, trk, 0});
            else
                tui_canvas_put(c, x, y, ' ', (tui_pen){t->fg, trk, 0});
        }
}

static void bar_set(tui_node *n, bar_state *st, float v)
{
    v = clamp01(v);
    tui_rect c = tui_get_content_rect(n);
    int a = eighths(st->value, c.w), b = eighths(v, c.w);
    st->value = v;
    if (a == b)
        return;
    int lo = (a < b ? a : b) / 8, hi = (a < b ? b : a) / 8;
    tui_invalidate_rect(n, (tui_rect){lo, 0, hi - lo + 1, c.h});
}

void tui_progress_set(tui_node *n, float v)
{
    bar_state *st = bar_of(n);
    if (st)
        bar_set(n, st, v);
}

void tui_progress_set_colors(tui_node *n, color_t fill, color_t track)
{
    bar_state *st = bar_of(n);
    if (!st)
        return;
    st->fill = fill;
    st->track = track;
    tui_invalidate(n);
}

float tui_progress_get(const tui_node *n)
{
    bar_state *st = bar_of(n);
    return st ? st->value : 0.0f;
}

static void slider_apply(tui_node *n, bar_state *st, float v)
{
    float old = st->value;
    bar_set(n, st, v);
    if (st->value != old && st->cb)
        st->cb(n, st->value, st->ud);
}

static float slider_value_at(tui_node *n, int lx)
{
    tui_rect r = tui_get_rect(n), c = tui_get_content_rect(n);
    int x = lx - (c.x - r.x);
    if (c.w <= 1)
        return 0.0f;
    return clamp01((float)x / (float)(c.w - 1));
}

static bool slider_event(tui_node *n, tui_event *ev, void *ud)
{
    bar_state *st = ud;
    switch (ev->type)
    {
    case TUI_EV_FOCUS:
    case TUI_EV_BLUR:
        tui_invalidate(n);
        return false;
    case TUI_EV_MOUSE_DOWN:
        if (ev->button != 0)
            return false;
        st->dragging = true;
        slider_apply(n, st, slider_value_at(n, ev->lx));
        return true;
    case TUI_EV_MOUSE_MOVE:
        if (!st->dragging)
            return false;
        slider_apply(n, st, slider_value_at(n, ev->lx));
        return true;
    case TUI_EV_MOUSE_UP:
        if (!st->dragging)
            return false;
        st->dragging = false;
        return true;
    case TUI_EV_KEY:
        switch (ev->key)
        {
        case TUI_KEY_LEFT:
        case TUI_KEY_DOWN: slider_apply(n, st, st->value - 0.05f); return true;
        case TUI_KEY_RIGHT:
        case TUI_KEY_UP:   slider_apply(n, st, st->value + 0.05f); return true;
        case TUI_KEY_HOME: slider_apply(n, st, 0.0f); return true;
        case TUI_KEY_END:  slider_apply(n, st, 1.0f); return true;
        default:           return false;
        }
    default:
        return false;
    }
}

tui_node *tui_progress(tui_ctx *ctx, float value, const tui_attr *attr)
{
    tui_attr a = {0};
    if (attr)
        a = *attr;
    bar_state *st = calloc(1, sizeof(*st));
    if (st == NULL)
        abort();
    st->value = clamp01(value);

    if (a.height.unit == TUI_UNIT_AUTO)
        a.height = TUI_CELLS(1);
    if (a.width.unit == TUI_UNIT_AUTO && a.grow == 0)
        a.grow = 1;
    a.paint = bar_paint;
    a.on_destroy = bar_free;
    a.userdata = st;

    tui_node *n = tui__open(ctx, TUI_KIND_BOX, &a);
    tui__close(ctx);
    return n;
}

tui_node *tui_slider(tui_ctx *ctx, float value, tui_value_fn on_change, void *ud,
                     const tui_attr *attr)
{
    tui_attr a = {0};
    if (attr)
        a = *attr;
    bar_state *st = calloc(1, sizeof(*st));
    if (st == NULL)
        abort();
    st->value = clamp01(value);
    st->interactive = true;
    st->cb = on_change;
    st->ud = ud;

    if (a.height.unit == TUI_UNIT_AUTO)
        a.height = TUI_CELLS(1);
    if (a.width.unit == TUI_UNIT_AUTO && a.grow == 0)
        a.grow = 1;
    a.focusable = true;
    a.paint = bar_paint;
    a.on_event = slider_event;
    a.on_destroy = bar_free;
    a.userdata = st;

    tui_node *n = tui__open(ctx, TUI_KIND_BOX, &a);
    tui__close(ctx);
    return n;
}
