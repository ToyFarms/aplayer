#include "tui_internal.h"

#include <stdio.h>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif
/*
 * Frame pipeline (tui_render):
 *
 *   layout pass        only dirty subtrees; reports moved/resized rects
 *   merge damage       coalesce rectangles, widen to whole wide glyphs
 *   paint              clear + repaint ONLY the damage rects into `back`
 *   present            per row, diff [min,max] of the damaged span against
 *                      `front`, emit runs with cursor/SGR tracking
 *
 * Cost is proportional to the damaged area (plus the nodes intersecting it),
 * never to the screen size.
 */

#define GAP_THROUGH 6 /* unchanged cells we rather re-send than jump over */

static const tui_cell BLANK = {' ', 0, 0, 0, 1};

/* ------------------------------------------------------------------------ */
/* buffers                                                                  */
/* ------------------------------------------------------------------------ */

int tui__screen_alloc(tui_ctx *ctx, int w, int h)
{
    size_t n = (size_t)w * (size_t)h;
    free(ctx->back);
    free(ctx->front);
    free(ctx->row_min);
    free(ctx->row_max);
    ctx->back = malloc(n * sizeof(tui_cell));
    ctx->front = malloc(n * sizeof(tui_cell));
    ctx->row_min = malloc((size_t)h * sizeof(int));
    ctx->row_max = malloc((size_t)h * sizeof(int));
    if (!ctx->back || !ctx->front || !ctx->row_min || !ctx->row_max)
        return -1;
    for (size_t i = 0; i < n; i++)
        ctx->back[i] = ctx->front[i] = BLANK;
    ctx->front_invalid = true;
    return 0;
}

void tui_force_redraw(tui_ctx *ctx)
{
    ctx->front_invalid = true;
}

/* ------------------------------------------------------------------------ */
/* output buffer                                                            */
/* ------------------------------------------------------------------------ */

void tui__out_put(tui_ctx *ctx, const char *s, size_t n)
{
    if (ctx->out_len + n > ctx->out_cap)
    {
        size_t cap = ctx->out_cap ? ctx->out_cap : 4096;
        while (cap < ctx->out_len + n)
            cap *= 2;
        ctx->out = realloc(ctx->out, cap);
        if (ctx->out == NULL)
            abort();
        ctx->out_cap = cap;
    }
    memcpy(ctx->out + ctx->out_len, s, n);
    ctx->out_len += n;
}

void tui__out_puts(tui_ctx *ctx, const char *s)
{
    tui__out_put(ctx, s, strlen(s));
}

static void out_uint(tui_ctx *ctx, unsigned v)
{
    char tmp[12];
    int n = 0;
    do
    {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    char rev[12];
    for (int i = 0; i < n; i++)
        rev[i] = tmp[n - 1 - i];
    tui__out_put(ctx, rev, (size_t)n);
}

static void out_utf8(tui_ctx *ctx, uint32_t cp)
{
    char b[4];
    int n;
    if (cp < 0x80)
    {
        b[0] = (char)cp;
        n = 1;
    }
    else if (cp < 0x800)
    {
        b[0] = (char)(0xC0 | (cp >> 6));
        b[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    }
    else if (cp < 0x10000)
    {
        b[0] = (char)(0xE0 | (cp >> 12));
        b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        b[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    }
    else
    {
        b[0] = (char)(0xF0 | (cp >> 18));
        b[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        b[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        b[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    tui__out_put(ctx, b, (size_t)n);
}

void tui__out_flush(tui_ctx *ctx)
{
    if (ctx->out_len == 0)
        return;

    if (ctx->opt.write)
    {
        ctx->opt.write(ctx->out, ctx->out_len, ctx->opt.write_ud);
    }
#ifdef _WIN32
    else
    {
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (out != INVALID_HANDLE_VALUE && out != NULL)
        {
            DWORD written;
            WriteFile(out, ctx->out, (DWORD)ctx->out_len, &written, NULL);
        }
    }
#else
    else
    {
        size_t off = 0;
        while (off < ctx->out_len)
        {
            ssize_t k = write(1, ctx->out + off, ctx->out_len - off);
            if (k <= 0)
                break;
            off += (size_t)k;
        }
    }
#endif

    ctx->out_len = 0;
}

/* ------------------------------------------------------------------------ */
/* SGR / cursor tracking                                                    */
/* ------------------------------------------------------------------------ */

typedef struct term_pen
{
    bool valid;
    uint32_t fg, bg;
    uint16_t attrs;
    int cx, cy; /* -1: unknown */
} term_pen;

static void sgr_sep(tui_ctx *ctx, bool *first)
{
    if (*first)
    {
        tui__out_put(ctx, "\x1b[", 2);
        *first = false;
    }
    else
        tui__out_put(ctx, ";", 1);
}

static void sgr_color(tui_ctx *ctx, bool *first, bool is_fg, uint32_t c)
{
    sgr_sep(ctx, first);
    if (c == 0)
    {
        tui__out_puts(ctx, is_fg ? "39" : "49");
        return;
    }
    color_t col = COLOR((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
    if (ctx->opt.color_mode == TUI_COLOR_256)
    {
        tui__out_puts(ctx, is_fg ? "38;5;" : "48;5;");
        out_uint(ctx, (unsigned)color_term_pallete(col));
    }
    else
    {
        tui__out_puts(ctx, is_fg ? "38;2;" : "48;2;");
        out_uint(ctx, col.r);
        tui__out_put(ctx, ";", 1);
        out_uint(ctx, col.g);
        tui__out_put(ctx, ";", 1);
        out_uint(ctx, col.b);
    }
}

static void set_pen(tui_ctx *ctx, term_pen *p, uint32_t fg, uint32_t bg,
                    uint16_t attrs)
{
    if (p->valid && p->fg == fg && p->bg == bg && p->attrs == attrs)
        return;

    bool first = true;
    bool reset = !p->valid || (p->attrs & (uint16_t)~attrs) != 0;
    if (reset)
    {
        sgr_sep(ctx, &first);
        tui__out_put(ctx, "0", 1);
        p->fg = p->bg = 0;
        p->attrs = 0;
    }
    static const struct
    {
        uint16_t bit;
        const char *code;
    } codes[] = {
        {TUI_BOLD,      "1"},
        {TUI_DIM,       "2"},
        {TUI_ITALIC,    "3"},
        {TUI_UNDERLINE, "4"},
        {TUI_REVERSE,   "7"},
        {TUI_STRIKE,    "9"}
    };
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
    {
        if ((attrs & codes[i].bit) && !(p->attrs & codes[i].bit))
        {
            sgr_sep(ctx, &first);
            tui__out_puts(ctx, codes[i].code);
        }
    }
    if (fg != p->fg)
        sgr_color(ctx, &first, true, fg);
    if (bg != p->bg)
        sgr_color(ctx, &first, false, bg);
    if (!first)
        tui__out_put(ctx, "m", 1);

    p->valid = true;
    p->fg = fg;
    p->bg = bg;
    p->attrs = attrs;
}

static int digits(unsigned v)
{
    int n = 1;
    while (v >= 10)
    {
        v /= 10;
        n++;
    }
    return n;
}

static void move_cursor(tui_ctx *ctx, term_pen *p, int x, int y)
{
    if (p->cx == x && p->cy == y)
        return;
    if (p->cy == y && p->cx >= 0)
    {
        int d = x - p->cx;
        int cup = 4 + digits((unsigned)y + 1) + digits((unsigned)x + 1);
        int rel = 3 + digits((unsigned)(d < 0 ? -d : d));
        if (rel < cup)
        {
            tui__out_put(ctx, "\x1b[", 2);
            out_uint(ctx, (unsigned)(d < 0 ? -d : d));
            tui__out_put(ctx, d < 0 ? "D" : "C", 1);
            p->cx = x;
            return;
        }
    }
    tui__out_put(ctx, "\x1b[", 2);
    out_uint(ctx, (unsigned)y + 1);
    tui__out_put(ctx, ";", 1);
    out_uint(ctx, (unsigned)x + 1);
    tui__out_put(ctx, "H", 1);
    p->cx = x;
    p->cy = y;
}

/* ------------------------------------------------------------------------ */
/* diff                                                                     */
/* ------------------------------------------------------------------------ */

static inline bool cell_eq(const tui_cell *a, const tui_cell *b)
{
    return a->cp == b->cp && a->fg == b->fg && a->bg == b->bg &&
           a->attrs == b->attrs && a->w == b->w;
}

static inline bool same_style(const tui_cell *c, const term_pen *p)
{
    return c->fg == p->fg && c->bg == p->bg && c->attrs == p->attrs;
}

static void diff_row(tui_ctx *ctx, term_pen *pen, int y, int x0, int x1)
{
    int W = ctx->width;
    tui_cell *back = ctx->back + (size_t)y * W;
    tui_cell *front = ctx->front + (size_t)y * W;

    /* never start or stop in the middle of a wide glyph */
    if (x0 > 0 && back[x0].w == 0)
        x0--;
    if (x1 + 1 < W && back[x1].w == 2)
        x1++;

    int x = x0;
    while (x <= x1)
    {
        /* find the next changed cell */
        while (x <= x1 && cell_eq(&back[x], &front[x]))
        {
            x++;
            ctx->cur.compared_cells++;
        }
        if (x > x1)
            return;
        if (back[x].w == 0 && x > 0)
            x--; /* continuation changed: resend its lead */

        move_cursor(ctx, pen, x, y);

        for (;;)
        {
            tui_cell *c = &back[x];
            int w = c->w ? c->w : 1;
            if (c->w == 0)
            {
                /* orphan continuation (should not happen): blank it */
                set_pen(ctx, pen, 0, 0, 0);
                tui__out_put(ctx, " ", 1);
            }
            else
            {
                set_pen(ctx, pen, c->fg, c->bg, c->attrs);
                out_utf8(ctx, c->cp);
            }
            for (int k = 0; k < w; k++)
                front[x + k] = back[x + k];
            ctx->cur.emitted_cells += (unsigned)w;
            ctx->cur.compared_cells += (unsigned)w;
            x += w;
            pen->cx += w;
            if (pen->cx >= W)
                pen->cx = -1; /* pending-wrap state: re-position next time */

            /* keep going through a short unchanged gap, or stop the run */
            int nx = x;
            while (nx <= x1 && cell_eq(&back[nx], &front[nx]))
                nx++;
            if (nx > x1)
                return;
            int gap = nx - x;
            bool through = pen->cx >= 0 && gap <= GAP_THROUGH;
            for (int g = x; through && g < nx; g++)
                if (!same_style(&back[g], pen) || back[g].w != 1)
                    through = false;
            if (!through)
            {
                ctx->cur.compared_cells += (unsigned)gap;
                x = nx;
                if (back[x].w == 0 && x > 0)
                    x--;
                move_cursor(ctx, pen, x, y);
            }
            /* else: fall through and write the gap cells as part of the run */
            if (through && x >= W)
                return;
        }
    }
}

/* Terminal cursor: shown on the caret of the node that asked for it (so the
 * IME anchors its pre-edit / candidate window there), hidden otherwise.  Only
 * emits what changed; `drew` means cell output moved the real cursor. */
static void emit_cursor(tui_ctx *ctx, bool drew)
{
    int x = 0, y = 0;
    bool vis = tui__cursor_resolve(ctx, &x, &y);
    if (!vis)
    {
        if (ctx->cur_emitted_vis)
            tui__out_puts(ctx, "\x1b[?25l");
        ctx->cur_emitted_vis = false;
        return;
    }
    if (ctx->cur_shape != ctx->cur_emitted_shape)
    {
        static const char *const shape[] = {"\x1b[0 q", "\x1b[2 q", "\x1b[6 q",
                                            "\x1b[4 q"};
        tui__out_puts(ctx, shape[ctx->cur_shape]);
        ctx->cur_emitted_shape = ctx->cur_shape;
    }
    if (drew || !ctx->cur_emitted_vis || x != ctx->cur_emitted_x ||
        y != ctx->cur_emitted_y)
    {
        tui__out_puts(ctx, "\x1b[");
        out_uint(ctx, (unsigned)y + 1);
        tui__out_put(ctx, ";", 1);
        out_uint(ctx, (unsigned)x + 1);
        tui__out_put(ctx, "H", 1);
        ctx->cur_emitted_x = x;
        ctx->cur_emitted_y = y;
    }
    if (!ctx->cur_emitted_vis)
        tui__out_puts(ctx, "\x1b[?25h");
    ctx->cur_emitted_vis = true;
}

static size_t present_cursor_only(tui_ctx *ctx)
{
    ctx->out_len = 0;
    emit_cursor(ctx, false);
    size_t n = ctx->out_len;
    tui__out_flush(ctx);
    return n;
}

size_t tui__screen_present(tui_ctx *ctx)
{
    int W = ctx->width, H = ctx->height;
    ctx->out_len = 0;

    if (ctx->opt.sync_output)
        tui__out_puts(ctx, "\x1b[?2026h");

    for (int y = 0; y < H; y++)
    {
        ctx->row_min[y] = W;
        ctx->row_max[y] = -1;
    }

    bool cleared = false;
    if (ctx->front_invalid)
    {
        cleared = true;
        tui__out_puts(ctx, "\x1b[0m\x1b[2J");
        for (size_t i = 0; i < (size_t)W * H; i++)
            ctx->front[i] = BLANK;
        for (int y = 0; y < H; y++)
        {
            ctx->row_min[y] = 0;
            ctx->row_max[y] = W - 1;
        }
        ctx->front_invalid = false;
    }
    else
    {
        for (int i = 0; i < ctx->ndamage; i++)
        {
            tui_rect r = ctx->damage[i];
            for (int y = r.y; y < r.y + r.h; y++)
            {
                if (r.x < ctx->row_min[y])
                    ctx->row_min[y] = r.x;
                if (r.x + r.w - 1 > ctx->row_max[y])
                    ctx->row_max[y] = r.x + r.w - 1;
            }
        }
    }

    term_pen pen = {false, 0, 0, 0, -1, -1};
    size_t header = ctx->out_len;
    for (int y = 0; y < H; y++)
    {
        if (ctx->row_max[y] < ctx->row_min[y])
            continue;
        diff_row(ctx, &pen, y, ctx->row_min[y], ctx->row_max[y]);
    }

    bool drew = cleared || ctx->out_len != header;
    if (pen.valid)
        tui__out_puts(ctx, "\x1b[0m");
    size_t mark = ctx->out_len;
    emit_cursor(ctx, drew);
    if (!drew && ctx->out_len == mark)
    {
        ctx->out_len = 0; /* nothing differed: send nothing at all */
        return 0;
    }
    if (ctx->opt.sync_output)
        tui__out_puts(ctx, "\x1b[?2026l");

    size_t n = ctx->out_len;
    ctx->cur.bytes = n;
    tui__out_flush(ctx);
    return n;
}

/* ------------------------------------------------------------------------ */
/* frame driver                                                             */
/* ------------------------------------------------------------------------ */

static long area(tui_rect r)
{
    return (long)r.w * r.h;
}

static void coalesce_damage(tui_ctx *ctx)
{
    bool merged = true;
    while (merged)
    {
        merged = false;
        for (int i = 0; i < ctx->ndamage && !merged; i++)
        {
            for (int j = i + 1; j < ctx->ndamage && !merged; j++)
            {
                tui_rect a = ctx->damage[i], b = ctx->damage[j];
                tui_rect u = tui__rect_union(a, b);
                /* merging must never repaint more than doing both */
                bool touch = a.x <= b.x + b.w && b.x <= a.x + a.w &&
                             a.y <= b.y + b.h && b.y <= a.y + a.h;
                if (touch && area(u) <= area(a) + area(b))
                {
                    ctx->damage[i] = u;
                    ctx->damage[j] = ctx->damage[--ctx->ndamage];
                    merged = true;
                }
            }
        }
    }
}

size_t tui_render(tui_ctx *ctx)
{
    memset(&ctx->cur, 0, sizeof(ctx->cur));

    /* on_resize callbacks may change the tree: lay out again (bounded) */
    for (int i = 0; i < 4; i++)
    {
        tui__layout_pass(ctx);
        if (!tui__fire_resize(ctx))
            break;
    }
    if (ctx->nrqueue == 0 && ctx->nlroots > 0)
        tui__layout_pass(ctx);

    if (ctx->front_invalid)
    {
        ctx->ndamage = 1;
        ctx->damage[0] = (tui_rect){0, 0, ctx->width, ctx->height};
    }

    if (ctx->ndamage == 0)
    {
        ctx->stats.layout_nodes = ctx->cur.layout_nodes;
        ctx->stats.painted_nodes = ctx->stats.damage_cells =
            ctx->stats.compared_cells = ctx->stats.emitted_cells =
                ctx->stats.bytes = 0;
        return present_cursor_only(ctx); /* caret moved without any damage */
    }

    coalesce_damage(ctx);

    /* One rect at a time: each is widened/validated against the buffer as the
     * previous ones left it, so glyph boundaries stay consistent. */
    for (int i = 0; i < ctx->ndamage; i++)
    {
        tui__paint_region(ctx, &ctx->damage[i]);
        ctx->cur.damage_cells += (uint64_t)area(ctx->damage[i]);
    }

    size_t n = tui__screen_present(ctx);
    ctx->ndamage = 0;

    uint64_t frames = ctx->stats.frames + (n ? 1 : 0);
    ctx->stats = ctx->cur;
    ctx->stats.frames = frames;
    return n;
}

tui_stats tui_get_stats(const tui_ctx *ctx)
{
    return ctx->stats;
}

/* ------------------------------------------------------------------------ */
/* terminal setup                                                           */
/* ------------------------------------------------------------------------ */

void tui_terminal_enter(tui_ctx *ctx, bool mouse)
{
    ctx->out_len = 0;
    tui__out_puts(ctx, "\x1b[?1049h\x1b[?25l\x1b[?7l");
    ctx->cur_emitted_vis = false;
    ctx->cur_emitted_shape = TUI_CURSOR_DEFAULT;
    if (mouse)
        tui__out_puts(ctx, "\x1b[?1003h\x1b[?1006h");
    tui__out_flush(ctx);
    ctx->front_invalid = true;
}

void tui_terminal_leave(tui_ctx *ctx)
{
    ctx->out_len = 0;
    tui__out_puts(ctx, "\x1b[0m\x1b[0 q\x1b[?1006l\x1b[?1003l\x1b[?7h\x1b[?25h"
                       "\x1b[?1049l");
    tui__out_flush(ctx);
}
