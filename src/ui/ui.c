#include "ui_internal.h"

#include "clock.h"

/*
 * Screen layout:
 *
 *   [ tab bar ]                                 row 0
 *   [ active view                          ]    grows
 *   (blank)
 *   timestamp ─ seek bar ─ gain                 row h-2
 *   status line    << [> >>        loudness     row h-1
 */

static void build_screen(ui_ctx *u)
{
    tui_ctx *ctx = u->tui;

    tui_attr root = tui_get_attr(tui_root(ctx));
    root.dir = TUI_COLUMN;
    tui_set_attr(tui_root(ctx), &root);

    u->tab_bar = ui_tab_bar_new(u);
    ui_views_build(u);
    u->transport = ui_transport_new(u, false);

    ui_view_select(u, UI_VIEW_PLAYLIST);
}

ui_ctx *ui_create(app_instance *app)
{
    ui_ctx *u = calloc(1, sizeof(*u));
    if (u == NULL)
        return NULL;
    u->app = app;
    u->base_theme = ui_theme_default();
    u->theme = u->base_theme;
    u->opt = ui_options_default();
    u->spectrum_mode = u->opt.spectrum.mode;
    u->color256 = app->term.capability.color != TERM_COLOR_24BIT;
    ui_ambience_init(&u->ambience, &u->base_theme.palette);
    u->skin_rev = u->ambience.palette_rev;

    vec2 size = term_size();
    tui_options opt = {
        .color_mode = app->term.capability.color == TERM_COLOR_24BIT
                          ? TUI_COLOR_TRUE
                          : TUI_COLOR_256,
        .sync_output = true,
    };
    u->tui = tui_create(size.x, size.y, &opt);
    if (u->tui == NULL)
    {
        free(u);
        return NULL;
    }

    tui_terminal_enter(u->tui, false); /* mouse reporting comes from term */
    tui_term_input_init(&u->input);
    u->router = tui_router_new(u->tui, &(tui_router_options){
                                          .timeout_ms = u->opt.keys.timeout_ms});
    tui_term_input_set_router(&u->input, u->router);
    if (u->opt.keys.langmap)
    {
        tui_langmap *lm = tui_router_langmap(u->router);
        tui_langmap_preset(lm, "ru");
        tui_langmap_preset(lm, "uk");
        tui_langmap_preset(lm, "el");
        tui_langmap_preset(lm, "cjk");
    }
    ui_hotkeys_init(u);
    ui_theme_apply_to_toolkit(&u->theme);
    build_screen(u);
    return u;
}

ui_font *ui_ctx_font(ui_ctx *u)
{
    if (!u->font_tried)
    {
        u->font_tried = true;
        u->font = ui_font_open_default(u->opt.lyrics.font);
        ui_font_set_weight(u->font, u->opt.lyrics.weight);
    }
    return u->font;
}

void ui_destroy(ui_ctx *u)
{
    if (u == NULL)
        return;
    tui_terminal_leave(u->tui);
    tui_router_free(u->router); /* before the nodes it watches go away */
    tui_term_input_free(&u->input);
    ui_hotkeys_destroy(u);
    tui_destroy(u->tui); /* releases every widget's state */
    ui_font_close(u->font);
    free(u);
}

bool ui_quit_requested(const ui_ctx *u)
{
    return u->quit;
}

void ui_event(ui_ctx *u, const term_event *e)
{
    if (e->type == TERM_EVENT_RESIZE)
        term_size_update(&u->app->term);

    tui_term_dispatch(&u->input, u->tui, e);
}

void ui_frame(ui_ctx *u)
{
    uint64_t now = gclock_now_ns();
    double dt =
        u->last_frame_ns == 0 ? 0.0 : (double)(now - u->last_frame_ns) / 1e9;
    u->last_frame_ns = now;

    tui_term_flush(&u->input, u->tui); /* end of this loop's input batch */
    tui_router_tick(u->router, dt);
    ui_search_sync(u->search);
    ui_skin_sync(u, dt);
    ui_transport_sync(u->transport);
    ui_transport_sync(u->transport_visual);
    tui_update(u->tui, dt);
    tui_render(u->tui);
}
