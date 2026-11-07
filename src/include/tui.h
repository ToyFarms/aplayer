#ifndef __TUI_H
#define __TUI_H

/*
 * tui - retained-mode, flexbox-laid-out, damage-tracked terminal UI.
 *
 * See docs/TUI.md for the design.  Short version:
 *
 *   - You build a tree of nodes from C (imperative API or the TUI_BOX /
 *     TUI_TEXT scope macros) and keep pointers to the nodes you want to change.
 *   - Any mutation (setters, append, remove, ...) only *marks* things dirty:
 *       layout dirt  -> propagated up to the nearest fixed-size ancestor
 *       paint dirt   -> turned into a screen-space damage rectangle
 *   - tui_render() re-lays-out only dirty subtrees, repaints only the damage
 *     rectangles into a back cell buffer, then diffs only those row spans
 *     against the front buffer and emits a minimal ANSI byte stream.
 *
 * The core has no dependency on the terminal layer: output goes through a
 * write callback and input comes in as tui_event (see tui_term.c for the
 * adapter from the project's term_event).
 */

#include "color.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* basic types                                                              */
/* ------------------------------------------------------------------------ */

typedef struct tui_ctx tui_ctx;
typedef struct tui_node tui_node;

typedef struct tui_rect
{
    int x, y, w, h;
} tui_rect;

typedef struct tui_size
{
    int w, h;
} tui_size;

typedef struct tui_edges
{
    int top, right, bottom, left;
} tui_edges;

#define TUI_EDGES_ALL(n)   ((tui_edges){(n), (n), (n), (n)})
#define TUI_EDGES_XY(x, y) ((tui_edges){(y), (x), (y), (x)})

/* Dimension: the zero value is "auto", so zero-initialised attrs just work. */
typedef enum tui_unit
{
    TUI_UNIT_AUTO = 0,
    TUI_UNIT_CELLS,
    TUI_UNIT_PERCENT,
} tui_unit;

typedef struct tui_dim
{
    float v;
    tui_unit unit;
} tui_dim;

#define TUI_AUTO     ((tui_dim){0.0f, TUI_UNIT_AUTO})
#define TUI_CELLS(n) ((tui_dim){(float)(n), TUI_UNIT_CELLS})
#define TUI_PCT(p)   ((tui_dim){(float)(p), TUI_UNIT_PERCENT})

/* text attribute bits */
#define TUI_BOLD      (1u << 0)
#define TUI_DIM       (1u << 1)
#define TUI_ITALIC    (1u << 2)
#define TUI_UNDERLINE (1u << 3)
#define TUI_REVERSE   (1u << 4)
#define TUI_STRIKE    (1u << 5)

/* ------------------------------------------------------------------------ */
/* style enums (zero value == CSS default)                                  */
/* ------------------------------------------------------------------------ */

typedef enum tui_kind
{
    TUI_KIND_BOX = 0,
    TUI_KIND_TEXT,
} tui_kind;

typedef enum tui_display
{
    TUI_DISPLAY_FLEX = 0,
    TUI_DISPLAY_NONE,
} tui_display;

typedef enum tui_direction
{
    TUI_ROW = 0,
    TUI_COLUMN,
    TUI_ROW_REVERSE,
    TUI_COLUMN_REVERSE,
} tui_direction;

typedef enum tui_wrap
{
    TUI_NOWRAP = 0,
    TUI_WRAP,
} tui_wrap;

typedef enum tui_justify
{
    TUI_JUSTIFY_START = 0,
    TUI_JUSTIFY_END,
    TUI_JUSTIFY_CENTER,
    TUI_JUSTIFY_BETWEEN,
    TUI_JUSTIFY_AROUND,
    TUI_JUSTIFY_EVENLY,
} tui_justify;

/* align-items: AUTO behaves as STRETCH.  align-self: AUTO inherits. */
typedef enum tui_align
{
    TUI_ALIGN_AUTO = 0,
    TUI_ALIGN_STRETCH,
    TUI_ALIGN_START,
    TUI_ALIGN_END,
    TUI_ALIGN_CENTER,
} tui_align;

typedef enum tui_align_content
{
    TUI_CONTENT_STRETCH = 0,
    TUI_CONTENT_START,
    TUI_CONTENT_END,
    TUI_CONTENT_CENTER,
    TUI_CONTENT_BETWEEN,
    TUI_CONTENT_AROUND,
} tui_align_content;

typedef enum tui_position
{
    TUI_POS_RELATIVE = 0, /* in normal flow */
    TUI_POS_ABSOLUTE,     /* out of flow, offsets from parent's padding box */
} tui_position;

typedef enum tui_border_style
{
    TUI_BORDER_NONE = 0,
    TUI_BORDER_SINGLE,
    TUI_BORDER_ROUNDED,
    TUI_BORDER_DOUBLE,
    TUI_BORDER_HEAVY,
    TUI_BORDER_ASCII,
} tui_border_style;

typedef enum tui_text_wrap
{
    TUI_TEXT_CLIP = 0, /* no wrap, cut at the edge */
    TUI_TEXT_ELLIPSIS, /* no wrap, "..." when cut */
    TUI_TEXT_WRAP,     /* word wrap (hard-breaks words longer than the line) */
} tui_text_wrap;

typedef enum tui_text_align
{
    TUI_TEXT_LEFT = 0,
    TUI_TEXT_CENTER,
    TUI_TEXT_RIGHT,
} tui_text_align;

/* flex-shrink: 0 in an attr means "default" (1).  Use this to really get 0. */
#define TUI_NO_SHRINK (-1.0f)

/* ------------------------------------------------------------------------ */
/* text helpers for custom widgets                                          */
/* ------------------------------------------------------------------------ */

/* Decode one UTF-8 sequence; returns bytes consumed (1 on invalid input with
 * cp = U+FFFD), 0 at end of input. */
size_t tui_utf8_decode(const char *s, size_t len, uint32_t *cp);
/* Encode a codepoint; returns bytes written (1-4). */
size_t tui_utf8_encode(uint32_t cp, char out[4]);
/* Terminal columns of a codepoint (0, 1 or 2) / of a UTF-8 string. */
int tui_cp_width(uint32_t cp);
int tui_text_width(const char *utf8, size_t len);

/* ------------------------------------------------------------------------ */
/* painting                                                                 */
/* ------------------------------------------------------------------------ */

/* Cell style used by custom paint callbacks. color a==0 means "default". */
typedef struct tui_pen
{
    color_t fg;
    color_t bg;
    uint16_t attrs;
} tui_pen;

/* Clipped drawing surface handed to custom paint callbacks.  Coordinates are
 * local to the node's content box (inside border + padding); (0,0) is its
 * top-left cell.  Anything outside the box / damage region is ignored. */
typedef struct tui_canvas tui_canvas;

int tui_canvas_width(const tui_canvas *c);
int tui_canvas_height(const tui_canvas *c);
/* Put one codepoint.  Returns the number of columns it occupies (0,1,2). */
int tui_canvas_put(tui_canvas *c, int x, int y, uint32_t cp, tui_pen pen);
/* Draw UTF-8 text on one line, clipped.  Returns columns drawn. */
int tui_canvas_text(tui_canvas *c, int x, int y, const char *utf8, tui_pen pen);
int tui_canvas_textn(tui_canvas *c, int x, int y, const char *utf8, size_t len,
                     tui_pen pen);
void tui_canvas_fill(tui_canvas *c, tui_rect r, uint32_t cp, tui_pen pen);
void tui_canvas_hline(tui_canvas *c, int x, int y, int len, uint32_t cp,
                      tui_pen pen);
void tui_canvas_vline(tui_canvas *c, int x, int y, int len, uint32_t cp,
                      tui_pen pen);

typedef void (*tui_paint_fn)(tui_node *self, tui_canvas *canvas, void *ud);

/* Intrinsic size of a custom leaf.  `avail_w` is the width the node will get
 * (>= 0), or -1 when asking for its max-content width. */
typedef tui_size (*tui_measure_fn)(tui_node *self, int avail_w, void *ud);

/* ------------------------------------------------------------------------ */
/* events                                                                   */
/* ------------------------------------------------------------------------ */

typedef enum tui_event_type
{
    TUI_EV_NONE = 0,
    TUI_EV_KEY,
    TUI_EV_MOUSE_DOWN,
    TUI_EV_MOUSE_UP,
    TUI_EV_MOUSE_MOVE,
    TUI_EV_SCROLL,
    TUI_EV_MOUSE_ENTER, /* not bubbled */
    TUI_EV_MOUSE_LEAVE, /* not bubbled */
    TUI_EV_FOCUS,       /* not bubbled */
    TUI_EV_BLUR,        /* not bubbled */
    TUI_EV_RESIZE,
    TUI_EV_TEXT,        /* text from an input method / paste (UTF-8, bubbles like a key) */
} tui_event_type;

#define TUI_MOD_SHIFT (1u << 0)
#define TUI_MOD_CTRL  (1u << 1)
#define TUI_MOD_ALT   (1u << 2)

/* printable keys are plain unicode codepoints, special keys live above it */
enum tui_key
{
    TUI_KEY_NONE = 0,
    TUI_KEY_ENTER = 0x110001,
    TUI_KEY_TAB,
    TUI_KEY_BACKSPACE,
    TUI_KEY_ESC,
    TUI_KEY_DELETE,
    TUI_KEY_INSERT,
    TUI_KEY_UP,
    TUI_KEY_DOWN,
    TUI_KEY_LEFT,
    TUI_KEY_RIGHT,
    TUI_KEY_HOME,
    TUI_KEY_END,
    TUI_KEY_PAGEUP,
    TUI_KEY_PAGEDOWN,
    TUI_KEY_F1,
    TUI_KEY_F2,
    TUI_KEY_F3,
    TUI_KEY_F4,
    TUI_KEY_F5,
    TUI_KEY_F6,
    TUI_KEY_F7,
    TUI_KEY_F8,
    TUI_KEY_F9,
    TUI_KEY_F10,
    TUI_KEY_F11,
    TUI_KEY_F12,
};

typedef struct tui_event
{
    tui_event_type type;
    int x, y;           /* screen cell (mouse events) */
    int lx, ly;         /* position relative to `current`'s border box */
    int button;         /* 0 left, 1 middle, 2 right */
    int scroll_delta;   /* >0 down / <0 up */
    uint32_t key;       /* codepoint or enum tui_key */
    uint32_t mod;       /* TUI_MOD_* */
    const char *text;   /* TUI_EV_TEXT: valid during dispatch only */
    size_t text_len;
    bool paste;         /* TUI_EV_TEXT came from a bracketed paste */
    int width, height;  /* TUI_EV_RESIZE */
    tui_node *target;   /* deepest node hit / focused node */
    tui_node *current;  /* node whose handler is running (bubbling) */
} tui_event;

/* Return true to consume the event (stops bubbling). */
typedef bool (*tui_handler_fn)(tui_node *self, tui_event *ev, void *ud);

/* Lifecycle hooks (all optional, all receive the node's `userdata`). */
/* Called by tui_update() once per tick for visible nodes. */
typedef void (*tui_update_fn)(tui_node *self, double dt, void *ud);
/* Called after layout when the node's content box changed size (and once
 * after the first layout).  May mutate the tree; a new layout pass follows. */
typedef void (*tui_resize_fn)(tui_node *self, tui_size content, void *ud);
/* Called right before the node is freed: release your state here. */
typedef void (*tui_destroy_fn)(tui_node *self, void *ud);

/* ------------------------------------------------------------------------ */
/* node attributes                                                          */
/* ------------------------------------------------------------------------ */

/* Everything about a node that can be described up-front.  Designed to be
 * written as a compound literal with designated initialisers; every field's
 * zero value is its default.  Read-modify-write on a live node with
 * tui_get_attr() / tui_set_attr(); only what really changed is invalidated. */
typedef struct tui_attr
{
    uint8_t _init; /* keeps `{._init = 0, __VA_ARGS__}` valid when empty */

    const char *id;   /* copied; use tui_find() to look the node up */
    const char *text; /* TUI_KIND_TEXT content, copied (UTF-8, '\n' ok) */

    /* --- as a flex container --- */
    tui_display display;
    tui_direction dir;
    tui_wrap wrap;
    tui_justify justify;
    tui_align align_items;
    tui_align_content align_content;
    int gap;     /* both axes ... */
    int row_gap; /* ... unless these are non-zero */
    int col_gap;

    /* --- as a flex item --- */
    float grow;
    float shrink; /* 0 => 1, TUI_NO_SHRINK => 0 */
    tui_dim basis;
    tui_align align_self;

    /* --- box model (border-box sizing) --- */
    tui_dim width, height;
    tui_dim min_w, min_h;
    tui_dim max_w, max_h;
    tui_edges margin;
    tui_edges padding;
    tui_border_style border;

    /* --- positioning --- */
    tui_position position;
    tui_dim left, top, right, bottom; /* absolute only */

    /* --- looks --- */
    color_t fg;        /* inherited when unset (alpha 0) */
    color_t bg;        /* transparent when unset */
    color_t border_fg; /* defaults to fg */
    uint16_t attrs;    /* TUI_BOLD ... (OR-ed with the parent's) */
    tui_text_wrap text_wrap;
    tui_text_align text_align;

    /* --- behaviour --- */
    tui_paint_fn paint;       /* drawn after bg/border, before children */
    tui_paint_fn paint_over;  /* drawn after the children; canvas = padding box
                                 (scrollbars, focus rings, badges) */
    tui_update_fn on_update;
    tui_resize_fn on_resize;
    tui_destroy_fn on_destroy;
    tui_measure_fn measure;   /* intrinsic size for custom leaves */
    tui_handler_fn on_event;
    void *userdata;           /* passed to paint / measure / on_event */
    bool focusable;
    bool scroll; /* scroll container: children never shrink below their size,
                    so content may overflow and be scrolled */
} tui_attr;

/* ------------------------------------------------------------------------ */
/* context                                                                  */
/* ------------------------------------------------------------------------ */

typedef void (*tui_write_fn)(const char *data, size_t len, void *ud);

typedef enum tui_color_mode
{
    TUI_COLOR_TRUE = 0, /* 24-bit */
    TUI_COLOR_256,
} tui_color_mode;

typedef struct tui_options
{
    tui_write_fn write; /* NULL => write(2) to fd 1 */
    void *write_ud;
    tui_color_mode color_mode;
    bool sync_output; /* wrap frames in DECSET 2026 (flicker-free) */
    bool soft_cursor; /* text widgets paint their caret instead of using the
                         terminal cursor (no IME anchoring) */
} tui_options;

typedef struct tui_stats
{
    uint64_t frames;           /* frames that produced output */
    uint64_t layout_nodes;     /* nodes laid out (last frame) */
    uint64_t painted_nodes;    /* nodes painted (last frame) */
    uint64_t damage_cells;     /* cells repainted (last frame) */
    uint64_t compared_cells;   /* cells diffed (last frame) */
    uint64_t emitted_cells;    /* cells written (last frame) */
    uint64_t bytes;            /* bytes written (last frame) */
} tui_stats;

tui_ctx *tui_create(int width, int height, const tui_options *opt);
void tui_destroy(tui_ctx *ctx);

/* The root is a column flex container that always fills the screen. */
tui_node *tui_root(tui_ctx *ctx);
void tui_resize(tui_ctx *ctx, int width, int height);
tui_size tui_screen_size(const tui_ctx *ctx);

/* Lay out, repaint damage, diff, emit.  Returns bytes written (0 = nothing
 * changed). */
size_t tui_render(tui_ctx *ctx);
/* Throw away the front buffer: the next render repaints and sends everything
 * (use after SIGCONT, an external clear, a terminal reset, ...). */
void tui_force_redraw(tui_ctx *ctx);
tui_stats tui_get_stats(const tui_ctx *ctx);

/* Terminal setup/teardown byte sequences (alt screen, hide cursor, no
 * autowrap, optional mouse).  Written through the write callback. */
void tui_terminal_enter(tui_ctx *ctx, bool mouse);
void tui_terminal_leave(tui_ctx *ctx);

/* ------------------------------------------------------------------------ */
/* node tree                                                                */
/* ------------------------------------------------------------------------ */

tui_node *tui_node_new(tui_ctx *ctx, tui_kind kind, const tui_attr *attr);
/* Detach (if attached) and destroy the node and its whole subtree. */
void tui_node_free(tui_node *node);

void tui_append(tui_node *parent, tui_node *child);
void tui_prepend(tui_node *parent, tui_node *child);
void tui_insert_before(tui_node *parent, tui_node *child, tui_node *ref);
void tui_insert_at(tui_node *parent, tui_node *child, int index);
/* Detach from the parent but keep the node alive (can be re-appended). */
void tui_detach(tui_node *node);
/* Free every child of `node`. */
void tui_clear_children(tui_node *node);

tui_node *tui_parent(const tui_node *n);
tui_node *tui_first_child(const tui_node *n);
tui_node *tui_last_child(const tui_node *n);
tui_node *tui_next_sibling(const tui_node *n);
tui_node *tui_prev_sibling(const tui_node *n);
int tui_child_count(const tui_node *n);
tui_node *tui_child_at(const tui_node *n, int index); /* O(index) */
tui_ctx *tui_node_ctx(const tui_node *n);
tui_kind tui_node_kind(const tui_node *n);

/* Lookup by id over every live node of the context (attached or not). */
tui_node *tui_find(tui_ctx *ctx, const char *id);

/* ------------------------------------------------------------------------ */
/* mutation                                                                 */
/* ------------------------------------------------------------------------ */

tui_attr tui_get_attr(const tui_node *n);
void tui_set_attr(tui_node *n, const tui_attr *attr);

/* Cheap shortcuts for the things that change every frame. */
void tui_set_text(tui_node *n, const char *utf8);
void tui_set_textf(tui_node *n, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
const char *tui_get_text(const tui_node *n);
void tui_set_fg(tui_node *n, color_t c);
void tui_set_bg(tui_node *n, color_t c);
void tui_set_visible(tui_node *n, bool visible);
void tui_set_size(tui_node *n, tui_dim w, tui_dim h);
void tui_set_grow(tui_node *n, float grow);
void tui_set_userdata(tui_node *n, void *ud);
void *tui_get_userdata(const tui_node *n);

/* Scroll offset of the node's children (clamped to the content extent after
 * layout).  Only repaints the node, never re-lays-out. */
void tui_set_scroll(tui_node *n, int x, int y);
void tui_scroll_by(tui_node *n, int dx, int dy);
tui_size tui_get_scroll(const tui_node *n);
/* Size of the scrollable content (valid after a render). */
tui_size tui_get_content_size(const tui_node *n);

/* Request a repaint without changing any property (custom paint callbacks
 * whose data changed).  The _rect form takes a rect in content coordinates
 * (same space as tui_canvas) so animated widgets can invalidate one bar. */
void tui_invalidate(tui_node *n);
void tui_invalidate_rect(tui_node *n, tui_rect content_rect);

/* Screen-space border box from the last layout. */
tui_rect tui_get_rect(const tui_node *n);
/* Screen-space content box (inside border + padding). */
tui_rect tui_get_content_rect(const tui_node *n);
bool tui_is_visible(const tui_node *n); /* attached, displayed, on screen tree */

/* Advance time: calls on_update(dt seconds) on every visible node that has one.
 * Call once per loop iteration, before tui_render(). */
void tui_update(tui_ctx *ctx, double dt);

/* Scroll every scrollable ancestor so that `n` is visible. */
void tui_scroll_into_view(tui_node *n);

/* pseudo-class style state, like :hover / :focus / :active */
bool tui_is_hovered(const tui_node *n);
bool tui_is_focused(const tui_node *n);
bool tui_is_pressed(const tui_node *n);

/* ------------------------------------------------------------------------ */
/* input                                                                    */
/* ------------------------------------------------------------------------ */

/* Routes mouse events by hit-testing, keys to the focused node (bubbling to
 * ancestors).  While a button is held, MOUSE_MOVE goes to the pressed node
 * (drag capture) and the matching MOUSE_UP is delivered to it as well.
 * Returns true if a handler consumed it.  RESIZE resizes. */
bool tui_dispatch(tui_ctx *ctx, tui_event *ev);
tui_node *tui_hit_test(tui_ctx *ctx, int x, int y);
void tui_focus(tui_ctx *ctx, tui_node *n);
tui_node *tui_focused(const tui_ctx *ctx);
void tui_focus_next(tui_ctx *ctx);
void tui_focus_prev(tui_ctx *ctx);

/* Like tui_dispatch for KEY/TEXT, but bubbling starts at `start` (NULL: only
 * the key hook's global step). No Tab handling. Used to replay keys. */
bool tui_dispatch_from(tui_ctx *ctx, tui_node *start, tui_event *ev);

/* Called for every KEY event at each step of the bubbling chain, BEFORE that
 * node's on_event (node == NULL: after the root, the "global" step).  Return
 * true to consume.  This is the hook the input router (tui_input.h) uses to
 * run per-node keymaps. */
typedef bool (*tui_key_hook_fn)(tui_node *node, tui_event *ev, void *ud);
void tui_set_key_hook(tui_ctx *ctx, tui_key_hook_fn fn, void *ud);
/* Called when a node is destroyed (drop references to it). */
typedef void (*tui_node_free_fn)(tui_node *node, void *ud);
void tui_set_node_free_hook(tui_ctx *ctx, tui_node_free_fn fn, void *ud);

/* ------------------------------------------------------------------------ */
/* terminal cursor (IME anchoring)                                          */
/* ------------------------------------------------------------------------ */

/* Terminals draw IME pre-edit text and candidate windows at the hardware
 * cursor.  A text widget therefore asks for the real cursor to sit on its
 * caret: `x`,`y` are in the node's content box coordinates.  At most one node
 * owns the cursor; it is hidden when the owner is not visible / clipped. */
typedef enum tui_cursor_shape
{
    TUI_CURSOR_DEFAULT = 0,
    TUI_CURSOR_BLOCK,
    TUI_CURSOR_BAR,
    TUI_CURSOR_UNDERLINE,
} tui_cursor_shape;

void tui_set_cursor(tui_node *n, int x, int y, tui_cursor_shape shape);
/* Release the cursor if `n` owns it. */
void tui_hide_cursor(tui_node *n);
/* Is the terminal cursor in use (false with options.soft_cursor)? */
bool tui_hw_cursor_enabled(const tui_ctx *ctx);

/* ------------------------------------------------------------------------ */
/* declarative building from C                                              */
/* ------------------------------------------------------------------------ */

/* Nodes created inside a scope are appended to the scope's node.
 *
 *   TUI_BOX(ctx, .dir = TUI_COLUMN, .grow = 1) {
 *       TUI_TEXT(ctx, "title", .attrs = TUI_BOLD);
 *       TUI_BOX(ctx, .id = "body", .grow = 1) { ... }
 *   }
 *
 * Do not `break`/`return`/`goto` out of a scope block.  tui_cursor() inside a
 * block is the node being built; tui_last() is the most recently created. */
tui_node *tui__open(tui_ctx *ctx, tui_kind kind, const tui_attr *attr);
void tui__close(tui_ctx *ctx);
tui_node *tui_cursor(tui_ctx *ctx);
tui_node *tui_last(tui_ctx *ctx);
tui_node *tui_build_push(tui_ctx *ctx, tui_node *under);
void tui_build_pop(tui_ctx *ctx, tui_node *prev);

#define TUI__CAT2(a, b) a##b
#define TUI__CAT(a, b)  TUI__CAT2(a, b)
#define TUI__SCOPE(ctx, kind, ...)                                             \
    for (int TUI__CAT(_tui_i_, __LINE__) =                                     \
             (tui__open((ctx), (kind), &(tui_attr){._init = 0, __VA_ARGS__}),  \
              0);                                                              \
         TUI__CAT(_tui_i_, __LINE__) == 0;                                     \
         TUI__CAT(_tui_i_, __LINE__) = (tui__close((ctx)), 1))

#define TUI_BOX(ctx, ...)  TUI__SCOPE(ctx, TUI_KIND_BOX, __VA_ARGS__)
#define TUI_TEXT(ctx, str, ...)                                                \
    do                                                                         \
    {                                                                          \
        tui__open((ctx), TUI_KIND_TEXT,                                        \
                  &(tui_attr){._init = 0, .text = (str), __VA_ARGS__});        \
        tui__close((ctx));                                                     \
    } while (0)
/* Build into an existing node (e.g. repopulate a list at runtime). */
#define TUI_UNDER(ctx, node)                                                   \
    for (tui_node *TUI__CAT(_tui_p_, __LINE__) = tui_build_push((ctx), (node)),\
                  *TUI__CAT(_tui_d_, __LINE__) = NULL;                         \
         TUI__CAT(_tui_d_, __LINE__) == NULL;                                  \
         TUI__CAT(_tui_d_, __LINE__) = (tui_build_pop(                         \
             (ctx), TUI__CAT(_tui_p_, __LINE__)),                              \
         (tui_node *)1))

#ifdef __cplusplus
}
#endif

#endif /* __TUI_H */
