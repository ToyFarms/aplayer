#include "ui_internal.h"

#include "clock.h"
#include "playlist.h"

#include <stdio.h>

/*
 * The "/" bar under the track list.
 *
 *   open     builds a fuzzy index of the track names, shows the bar and pushes
 *            a modal focus layer (the text input gets the keys, Esc / Enter /
 *            arrows come from the search keymap in ui_hotkeys.c)
 *   typing   every change starts a fuzzy_search. Extending the query only
 *            re-scores the previous matches; the first slice runs right away
 *            (so results are on screen this frame), a huge library keeps
 *            scanning a few ms per frame in ui_search_sync()
 *   close    pops the layer (focus goes back), drops the filter
 *
 * The playlist shows the hits through a ui_filter; selecting/playing happens
 * through ui_action_search_*() so mouse and keyboard share it.
 */

struct ui_search
{
    ui_ctx *u;
    tui_node *bar, *input, *count;
    bool active;

    fuzzy_index *index;
    fuzzy_search *cur;
    ui_filter filter;
    bool moved; /* the user picked a row: do not reset it when sorting ends */

    int saved_index; /* playlist selection when the bar opened */
    int shown_hits, shown_total;
    bool shown_done;
};

static uint64_t slice_ns(const ui_search *s)
{
    return (uint64_t)s->u->opt.search.slice_us * 1000ull;
}

static void update_count(ui_search *s)
{
    int total = (int)s->u->app->playlist.files.length;
    size_t n = 0;
    bool done = true;
    if (s->cur)
    {
        fuzzy_search_hits(s->cur, &n);
        done = fuzzy_search_done(s->cur);
    }
    else
        n = (size_t)total;
    if ((int)n == s->shown_hits && total == s->shown_total && done == s->shown_done)
        return;
    s->shown_hits = (int)n;
    s->shown_total = total;
    s->shown_done = done;
    tui_set_textf(s->count, " %d/%d%s ", (int)n, total, done ? "" : "…");
}

/* hand the current hits to the playlist */
static void publish(ui_search *s, bool reset_cursor)
{
    size_t n = 0;
    const fuzzy_hit *h = fuzzy_search_hits(s->cur, &n);
    s->filter.hits = h;
    s->filter.count = (int)n;
    s->filter.rev++;
    s->filter.query = fuzzy_search_query(s->cur);
    ui_playlist_set_filter(s->u->playlist, &s->filter, reset_cursor);
}

static void query_changed(tui_node *n, const char *text, void *ud)
{
    (void)n;
    ui_search *s = ud;
    if (!s->active)
        return;

    int selected = ui_playlist_cursor(s->u->playlist); /* before old hits die */
    fuzzy_search *old = s->cur;
    fuzzy_search *next = fuzzy_search_new(
        s->index, text, old && fuzzy_search_done(old) ? old : NULL);
    fuzzy_search_free(old); /* the new search copied what it needed */
    s->cur = next;
    s->moved = false;

    if (fuzzy_query_empty(fuzzy_search_query(next)))
    {
        /* nothing typed: the whole list, selection stays where it is */
        fuzzy_search_free(next);
        s->cur = NULL;
        ui_playlist_clear_filter(s->u->playlist, selected);
        update_count(s);
        return;
    }
    fuzzy_search_run(next, slice_ns(s));
    publish(s, true);
    update_count(s);
}

void ui_search_sync(ui_search *s)
{
    if (!s->active || s->cur == NULL || fuzzy_search_done(s->cur))
        return;
    bool done = fuzzy_search_run(s->cur, slice_ns(s));
    publish(s, done && !s->moved); /* sorted now: best match under the cursor */
    update_count(s);
}

void ui_search_moved(ui_search *s)
{
    s->moved = true;
}

bool ui_search_active(const ui_search *s)
{
    return s->active;
}

static void build_index(ui_search *s)
{
    playlist_manager *pl = &s->u->app->playlist;
    int n = (int)pl->files.length;
    fuzzy_index_free(s->index);
    s->index = fuzzy_index_new();
    fuzzy_index_reserve(s->index, (size_t)n, 32);
    for (int i = 0; i < n; i++)
    {
        const fs_entry_t *e = playlist_get_at_index(pl, i);
        if (e)
            fuzzy_index_add(s->index, e->name.buf, e->name.len);
        else
            fuzzy_index_add(s->index, "", 0); /* keep ids == playlist index */
    }
}

void ui_search_open(ui_search *s)
{
    if (s->active)
        return;
    ui_ctx *u = s->u;
    if (u->view != UI_VIEW_PLAYLIST)
        ui_view_select(u, UI_VIEW_PLAYLIST);

    build_index(s);
    s->saved_index = ui_playlist_cursor(u->playlist);
    s->active = true;
    s->moved = false;
    s->shown_hits = -1;
    tui_input_set_text(s->input, "");
    tui_set_visible(s->bar, true);
    update_count(s);

    tui_router_push_layer(u->router, &(tui_layer_desc){
                                        .focus = s->input,
                                        .keymap = u->km_search,
                                        .mode = u->mode_search,
                                        .modal = true,
                                    });
}

void ui_search_close(ui_search *s, bool restore, int select)
{
    if (!s->active)
        return;
    ui_ctx *u = s->u;
    s->active = false;
    tui_router_pop_layer(u->router); /* focus returns to what it was */
    tui_set_visible(s->bar, false);

    fuzzy_search_free(s->cur);
    s->cur = NULL;
    fuzzy_index_free(s->index);
    s->index = NULL;
    ui_playlist_clear_filter(u->playlist, restore ? s->saved_index : select);
}

static void search_free(tui_node *n, void *ud)
{
    (void)n;
    ui_search *s = ud;
    fuzzy_search_free(s->cur);
    fuzzy_index_free(s->index);
    free(s);
}

ui_search *ui_search_new(ui_ctx *u)
{
    ui_search *s = calloc(1, sizeof(*s));
    if (s == NULL)
        abort();
    s->u = u;
    tui_ctx *ctx = u->tui;
    const ui_theme *t = &u->theme;

    TUI_BOX(ctx, .height = TUI_CELLS(1), .shrink = TUI_NO_SHRINK,
            .display = TUI_DISPLAY_NONE, .bg = t->search.bg, .on_destroy = search_free,
            .userdata = s)
    {
        s->bar = tui_cursor(ctx);
        TUI_TEXT(ctx, "/", .fg = t->search.prompt_fg, .bg = t->search.bg,
                 .attrs = TUI_BOLD, .padding = {0, 0, 0, 1}, .shrink = TUI_NO_SHRINK);
        s->input = tui_input(ctx, "",
                             &(tui_attr){._init = 0,
                                         .grow = 1,
                                         .fg = t->search.fg,
                                         .bg = t->search.bg});
        tui_input_set_placeholder(s->input, "fuzzy search");
        tui_input_on_change(s->input, query_changed, s);
        TUI_TEXT(ctx, "", .fg = t->search.count_fg, .bg = t->search.bg,
                 .shrink = TUI_NO_SHRINK);
        s->count = tui_last(ctx);
    }
    return s;
}
