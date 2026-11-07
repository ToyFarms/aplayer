#ifndef __TUI_INTERNAL_H
#define __TUI_INTERNAL_H

#include "tui.h"

#include <stdlib.h>
#include <string.h>

#define TUI_MAX_DAMAGE 32
#define TUI_HFW_CACHE  4

/* node flags */
#define NF_LAYOUT_DIRTY (1u << 0) /* subtree needs relayout */
#define NF_QUEUED       (1u << 1) /* sits in ctx->lroots */
#define NF_MC_VALID     (1u << 2) /* cached max-content width is valid */
#define NF_LAID_OUT     (1u << 3) /* `rect` reflects something on screen */
#define NF_IS_ROOT      (1u << 4)
#define NF_IN_ID_TABLE  (1u << 5)
#define NF_TICKING      (1u << 6) /* registered in ctx->tickers */
#define NF_RESIZE_Q     (1u << 7) /* sits in ctx->rqueue */

/* One terminal cell.  16 bytes, no padding, compared field by field. */
typedef struct tui_cell
{
    uint32_t cp;    /* 0xFFFFFFFF = "unknown" (forces emit) */
    uint32_t fg;    /* packed ARGB, alpha 0 == terminal default */
    uint32_t bg;
    uint16_t attrs;
    uint16_t w;     /* 1 normal, 2 wide lead, 0 continuation of a wide lead */
} tui_cell;

#define TUI_CP_UNKNOWN 0xFFFFFFFFu

struct tui_node
{
    tui_ctx *ctx;
    tui_kind kind;
    uint32_t flags;

    tui_node *parent, *first, *last, *next, *prev;
    int nchildren;

    char *id_str; /* owned */
    char *text;   /* owned, NUL terminated */
    size_t text_len;
    tui_attr a; /* a.id / a.text alias id_str / text */

    /* layout result: border box relative to the parent's border-box origin
     * (children are NOT pre-offset by the parent's scroll) */
    tui_rect rect;
    int scroll_x, scroll_y;
    int rs_w, rs_h; /* content size last reported to on_resize */
    int content_w, content_h; /* extent of in-flow children (for scrolling) */

    /* intrinsic size caches, invalidated by tui__mark_layout */
    int mcw;
    int hfw_w[TUI_HFW_CACHE];
    int hfw_h[TUI_HFW_CACHE];
    uint8_t hfw_n, hfw_next;
};

struct tui_ctx
{
    tui_options opt;
    int width, height;
    tui_node *root;

    /* cell buffers */
    tui_cell *back;
    tui_cell *front;
    bool front_invalid; /* next render must clear + resend everything */

    /* damage (screen space) */
    tui_rect damage[TUI_MAX_DAMAGE];
    int ndamage;

    /* layout roots queue */
    tui_node **lroots;
    int nlroots, lroots_cap;

    /* id table (open addressing, tombstone = (tui_node *)1) */
    tui_node **ids;
    int ids_cap, ids_count, ids_used;

    /* input state */
    tui_node *focus;
    tui_node *hover;
    tui_node *pressed;
    tui_node *consumer; /* handler that consumed the last bubbled event */
    tui_key_hook_fn key_hook;
    void *key_hook_ud;
    tui_node_free_fn free_hook;
    void *free_hook_ud;

    /* terminal cursor */
    tui_node *cur_owner;
    int cur_x, cur_y;
    tui_cursor_shape cur_shape;
    bool cur_emitted_vis; /* what the terminal currently shows */
    int cur_emitted_x, cur_emitted_y;
    tui_cursor_shape cur_emitted_shape;

    /* nodes with on_update */
    tui_node **tickers;
    int ntickers, tickers_cap;
    bool ticking; /* inside tui_update: removal leaves NULL holes */

    /* nodes waiting for their on_resize callback */
    tui_node **rqueue;
    int nrqueue, rqueue_cap;

    /* builder */
    tui_node **bstack;
    int bdepth, bcap;
    tui_node *blast;

    /* output */
    char *out;
    size_t out_len, out_cap;

    /* set by the canvas when the damage region cut a wide glyph */
    bool cut;
    tui_rect cut_rect;

    /* per-row dirty spans while diffing */
    int *row_min, *row_max;

    tui_stats stats;
    tui_stats cur; /* being accumulated for the frame in flight */
};

/* ---- util ---- */
static inline int tui__min(int a, int b) { return a < b ? a : b; }
static inline int tui__max(int a, int b) { return a > b ? a : b; }
static inline int tui__clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline bool tui__rect_empty(tui_rect r) { return r.w <= 0 || r.h <= 0; }
tui_rect tui__rect_intersect(tui_rect a, tui_rect b);
tui_rect tui__rect_union(tui_rect a, tui_rect b);
static inline bool tui__rect_contains(tui_rect o, tui_rect i)
{
    return i.x >= o.x && i.y >= o.y && i.x + i.w <= o.x + o.w &&
           i.y + i.h <= o.y + o.h;
}
uint32_t tui__pack(color_t c);

/* ---- node.c ---- */
static inline int tui__border_w(const tui_node *n)
{
    return n->a.border != TUI_BORDER_NONE ? 1 : 0;
}
/* total space between border-box edge and content box */
tui_edges tui__insets(const tui_node *n);
void tui__mark_layout(tui_node *n);
void tui__invalidate_caches(tui_node *n);
void tui__invalidate_chain(tui_node *n);
void tui__damage_add(tui_ctx *ctx, tui_rect abs);
/* border box in screen coordinates, from the last committed layout */
tui_rect tui__abs_rect(const tui_node *n);
/* screen position of n's border-box origin */
void tui__abs_origin(const tui_node *n, int *x, int *y);
bool tui__attached(const tui_node *n);
void tui__id_unregister(tui_node *n);
void tui__id_set(tui_node *n, const char *id);
void tui__destroy_subtree(tui_node *n);
/* damage the node's current on-screen border box (no-op if not on screen) */
void tui__damage_node(tui_node *n);
void tui__tick_sync(tui_node *n);   /* (un)register on_update after attr change */
void tui__resize_enqueue(tui_node *n);
void tui__resize_unqueue(tui_node *n);
void tui__tick_unregister(tui_node *n);
bool tui__fire_resize(tui_ctx *ctx); /* true if any callback ran */

/* ---- text.c ---- */
size_t tui__utf8_decode(const char *p, size_t len, uint32_t *cp);
int tui__cp_width(uint32_t cp);
int tui__text_width(const char *s, size_t len);
int tui__text_max_width(const tui_node *n);
int tui__text_min_width(const tui_node *n);
int tui__text_height_for_width(const tui_node *n, int w);
void tui__text_paint(const tui_node *n, tui_canvas *cv, tui_pen pen);

/* ---- layout.c ---- */
void tui__layout_pass(tui_ctx *ctx);
int tui__max_content_w(tui_node *n);
int tui__height_for_width(tui_node *n, int w);
/* flex helpers shared with tui_measure.c */
int tui__resolve(tui_dim d, int base);
int tui__gap_main(const tui_node *n);
int tui__gap_cross(const tui_node *n);
/* auto border-box height of a container laid out at border-box width W,
 * without touching any node state */
int tui__flex_height(tui_node *n, int W);

/* ---- paint.c ---- */
struct tui_canvas
{
    tui_ctx *ctx;
    tui_rect box;   /* content box, screen space (local origin) */
    tui_rect clip;  /* structural clip AND the damage region being painted */
    tui_rect sclip; /* structural clip only (node / ancestor boxes, screen) */
};
/* Clear + repaint `*region`.  Widens it (and repaints) until no wide glyph is
 * cut by its edges; the final region is written back. */
void tui__paint_region(tui_ctx *ctx, tui_rect *region);
void tui__buf_put(tui_ctx *ctx, int x, int y, uint32_t cp, int w, uint32_t fg,
                  uint32_t bg, uint16_t attrs, bool keep_bg);
void tui__buf_clear(tui_ctx *ctx, tui_rect r);

/* ---- screen.c ---- */
int tui__screen_alloc(tui_ctx *ctx, int w, int h);
void tui__out_put(tui_ctx *ctx, const char *s, size_t n);
void tui__out_puts(tui_ctx *ctx, const char *s);
void tui__out_flush(tui_ctx *ctx);
size_t tui__screen_present(tui_ctx *ctx);

/* ---- event.c ---- */
void tui__events_forget(tui_ctx *ctx, tui_node *n); /* node is going away */
/* resolves the cursor request to a screen cell; false = hidden */
bool tui__cursor_resolve(tui_ctx *ctx, int *x, int *y);

#endif /* __TUI_INTERNAL_H */
