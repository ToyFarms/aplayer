#include "ui_internal.h"

#include "playlist.h"

/*
 * Playlist list: one custom-paint node. Owns the cursor and the scroll
 * position; the playing track and its order belong to the app.
 *
 *   click / wheel   move the cursor          (widget events)
 *   hotkeys         move / jump the cursor   (ui_playlist_* API)
 *   new track       cursor jumps to it       (observed via app->track_serial)
 *   search filter   rows are a subset of the tracks, best match first
 *                   (ui_filter, set by the search bar); matched characters are
 *                   highlighted. Without a filter row == playlist index.
 */

struct ui_playlist
{
    ui_ctx *u;
    tui_node *node;

    const ui_filter *filter; /* NULL: every track */

    int cursor;         /* selected row */
    int offset;         /* first visible row */
    bool recenter;      /* put the cursor in the middle on the next update */
    bool force_repaint; /* order changed behind our back */
    uint32_t track_serial;

    /* Last painted signature: invalidate only when something visible moved. */
    int sig_cursor, sig_playing, sig_offset, sig_count, sig_h, sig_w;
    uint32_t sig_rev;
};

static int file_count(const ui_playlist *p)
{
    return (int)p->u->app->playlist.files.length;
}

static int row_count(const ui_playlist *p)
{
    return p->filter ? p->filter->count : file_count(p);
}

/* playlist index shown on a row */
static int row_index(const ui_playlist *p, int row)
{
    return p->filter ? (int)p->filter->hits[row].id : row;
}

/* ------------------------------------------------------------------------ */
/* public API                                                               */
/* ------------------------------------------------------------------------ */

int ui_playlist_cursor(const ui_playlist *p)
{
    int n = row_count(p);
    if (n == 0)
        return p->filter ? -1 : 0;
    return row_index(p, MATH_CLAMP(p->cursor, 0, n - 1));
}

int ui_playlist_cursor_row(const ui_playlist *p)
{
    return p->cursor;
}

bool ui_playlist_filtered(const ui_playlist *p)
{
    return p->filter != NULL;
}

void ui_playlist_set_cursor(ui_playlist *p, int row)
{
    int n = row_count(p);
    if (n == 0)
        return;
    p->cursor = MATH_CLAMP(row, 0, n - 1);
}

void ui_playlist_recenter(ui_playlist *p)
{
    p->recenter = true;
}

void ui_playlist_set_filter(ui_playlist *p, const ui_filter *f, bool reset_cursor)
{
    p->filter = f;
    p->force_repaint = true;
    if (reset_cursor)
    {
        p->cursor = 0;
        p->offset = 0;
    }
    else
        p->cursor = MATH_CLAMP(p->cursor, 0, MATH_MAX(row_count(p) - 1, 0));
}

void ui_playlist_clear_filter(ui_playlist *p, int select)
{
    p->filter = NULL;
    p->force_repaint = true;
    p->recenter = true;
    if (select >= 0)
        p->cursor = MATH_CLAMP(select, 0, MATH_MAX(file_count(p) - 1, 0));
}

void ui_playlist_move_cursor(ui_playlist *p, int delta)
{
    ui_playlist_set_cursor(p, p->cursor + delta);
}

void ui_playlist_follow_playing(ui_playlist *p)
{
    app_instance *app = p->u->app;
    if (p->track_serial == app->track_serial)
        return;

    p->track_serial = app->track_serial;
    if (p->filter == NULL) /* the selection is the user's while searching */
        p->cursor = app->playlist.current_idx;
}

void ui_playlist_reordered(ui_playlist *p)
{
    p->force_repaint = true;
    p->recenter = true;
    p->cursor = p->u->app->playlist.current_idx;
}

/* ------------------------------------------------------------------------ */
/* toolkit hooks                                                            */
/* ------------------------------------------------------------------------ */

static void pl_free(tui_node *n, void *ud)
{
    (void)n;
    free(ud);
}

/* scrolloff / recenter behaviour of the original list widget */
static void pl_update(tui_node *n, double dt, void *ud)
{
    (void)dt;
    ui_playlist *p = ud;
    ui_ctx *u = p->u;
    int count = row_count(p);
    tui_rect c = tui_get_content_rect(n);
    int h = c.h;
    int scrolloff = u->opt.playlist.scrolloff;

    ui_playlist_follow_playing(p);

    if (count > 0 && h > 0)
    {
        p->cursor = MATH_CLAMP(p->cursor, 0, count - 1);
        int leftover = MATH_MAX(count - h, 0);
        int rel = p->cursor - p->offset;

        if (rel > h - scrolloff)
            p->offset =
                MATH_CLAMP(p->offset + (rel - (h - scrolloff)), 0, leftover);
        else if (rel < scrolloff)
            p->offset = MATH_MAX(p->offset - (scrolloff - rel) + 1, 0);

        if (p->recenter)
        {
            p->offset = p->cursor - h / 2;
            p->recenter = false;
        }
        p->offset = MATH_CLAMP(p->offset, 0, leftover);
    }

    int playing = u->app->playlist.current_idx;
    if (p->sig_cursor != p->cursor || p->sig_playing != playing ||
        p->sig_offset != p->offset || p->sig_count != count || p->sig_h != h ||
        p->sig_w != c.w || p->force_repaint ||
        p->sig_rev != (p->filter ? p->filter->rev : 0))
    {
        p->sig_cursor = p->cursor;
        p->sig_playing = playing;
        p->sig_offset = p->offset;
        p->sig_count = count;
        p->sig_h = h;
        p->sig_w = c.w;
        p->sig_rev = p->filter ? p->filter->rev : 0;
        p->force_repaint = false;
        tui_invalidate(n);
    }
}

/* The track name; with a search active the matched characters stand out. */
static void paint_name(tui_canvas *cv, int x, int y, int W, const fs_entry_t *e,
                       const ui_filter *f, tui_pen pen, color_t match_fg)
{
    uint16_t pos[96];
    int np = f && f->query ? fuzzy_query_positions(f->query, e->name.buf,
                                                    e->name.len, pos, 96)
                           : 0;
    if (np <= 0)
    {
        tui_canvas_textn(cv, x, y, e->name.buf, e->name.len, pen);
        return;
    }
    tui_pen hl = pen;
    hl.fg = match_fg;
    hl.attrs |= TUI_BOLD | TUI_UNDERLINE;

    const char *s = e->name.buf;
    size_t len = e->name.len, i = 0;
    int ci = 0, next = 0;
    while (i < len && x < W)
    {
        uint32_t cp;
        size_t k = tui_utf8_decode(s + i, len - i, &cp);
        if (k == 0)
            break;
        bool hit = next < np && pos[next] == ci;
        if (hit)
            next++;
        int w = tui_cp_width(cp);
        if (w > 0)
            x += tui_canvas_put(cv, x, y, cp, hit ? hl : pen);
        i += k;
        ci++;
    }
}

static void pl_paint(tui_node *n, tui_canvas *cv, void *ud)
{
    (void)n;
    ui_playlist *p = ud;
    ui_ctx *u = p->u;
    const ui_theme *t = &u->theme;
    int W = tui_canvas_width(cv), H = tui_canvas_height(cv);
    int count = row_count(p);

    for (int y = 0; y < H; y++)
    {
        int row = p->offset + y;
        if (row >= count)
            break;
        int idx = row_index(p, row);

        bool playing = idx == u->app->playlist.current_idx;
        bool selected = row == p->cursor;

        tui_pen pen = ui_pen(t->list.normal_fg, t->list.normal_bg);
        if (playing)
            pen = ui_pen(t->list.playing_fg, t->list.playing_bg);
        else if (selected)
            pen.attrs |= TUI_REVERSE;

        char num[16];
        int nlen = snprintf(num, sizeof(num), "%5d ", idx);
        tui_canvas_text(cv, 0, y, num,
                        ui_pen(t->list.number_fg, t->list.number_bg));

        tui_canvas_fill(cv, (tui_rect){nlen, y, W - nlen, 1}, ' ', pen);
        fs_entry_t *e = playlist_get_at_index(&u->app->playlist, idx);
        if (e != NULL)
        {
            tui_canvas_put(cv, nlen, y, ' ', pen);
            paint_name(cv, nlen + 1, y, W, e, p->filter, pen, t->list.match_fg);
        }
    }
}

static bool pl_event(tui_node *n, tui_event *ev, void *ud)
{
    (void)n;
    ui_playlist *p = ud;
    switch (ev->type)
    {
    case TUI_EV_SCROLL:
        ui_playlist_move_cursor(p, ev->scroll_delta * 3);
        if (p->filter)
            ui_search_moved(p->u->search);
        return true;
    case TUI_EV_MOUSE_DOWN:
        if (ev->button == 0)
        {
            ui_playlist_set_cursor(p, p->offset + ev->ly);
            if (p->filter)
                ui_search_moved(p->u->search);
            return true;
        }
        return false;
    default:
        return false;
    }
}

ui_playlist *ui_playlist_new(ui_ctx *u)
{
    ui_playlist *p = calloc(1, sizeof(*p));
    if (p == NULL)
        abort();
    p->u = u;
    p->recenter = true; /* start on the restored track, not at the top */
    p->sig_cursor = p->sig_playing = p->sig_offset = p->sig_count = -1;

    p->node = tui__open(u->tui, TUI_KIND_BOX,
                        &(tui_attr){._init = 0,
                                    .grow = 1,
                                    .bg = u->theme.list.normal_bg,
                                    .paint = pl_paint,
                                    .on_update = pl_update,
                                    .on_event = pl_event,
                                    .on_destroy = pl_free,
                                    .userdata = p});
    tui__close(u->tui);
    return p;
}
