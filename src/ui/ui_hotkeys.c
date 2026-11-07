#include "ui_internal.h"

/*
 * Keymaps. Bindings run through the input router (tui_router.h), which handles
 * multi-key sequences, counts, timeouts and non-Latin layouts; this file only
 * says which keys call which ui_action_*().
 *
 * NORMAL (global, consulted when no focused widget consumed the key):
 *   q ZZ ^C       quit                    j/k, down/up   move cursor (3j = 3 rows)
 *   enter         play selected           ^u/^d, {/}     half page up / down
 *                 (shift: add to queue)   gg / G, Home/End  first / last
 *   space         pause                   zz             centre the cursor /
 *                                                          lyrics follow the music
 *   [ / ]         previous / next cover   (j/k, pages scroll the lyrics on
 *                                          the VISUAL and METADATA views)
 *   N / P         next / previous         left / right   seek -/+ 2.5 s
 *   ^j / ^k       gain -/+ 0.5 dB         m, r, s, d     mute, shuffle, sort, direction
 *   1-9, gt/gT    switch view             ^l             redraw
 *   /             search                  F1             cover art method
 *   w             spectrum: filled line / line / oscilloscope
 *   + / -         big lyric line one row taller / shorter (3+ = 3 rows)
 *   =             big lyric line back to its default size
 *
 * SEARCH (modal layer while the / bar is open; typing goes to the input):
 *   enter         play selection, back to the full list
 *   esc, ^C       cancel
 *   down ^n ^j tab    next result         up ^p ^k S-tab    previous result
 *   pgdn / pgup       half page
 */

#define SEEK_STEP_MS 2500
#define GAIN_STEP_DB 0.5f

static int times(const tui_keyctx *kc)
{
    return kc->has_count && kc->count > 0 ? kc->count : 1;
}

static int half_page(ui_ctx *u)
{
    return MATH_MAX(tui_screen_size(u->tui).h / 2, 1);
}

#define ACTION(name, body)                                                     \
    static bool name(const tui_keyctx *kc, void *ud)                           \
    {                                                                          \
        ui_ctx *u = ud;                                                        \
        (void)kc;                                                              \
        (void)u;                                                               \
        body;                                                                  \
        return true;                                                           \
    }

ACTION(k_quit, ui_action_quit(u))
ACTION(k_down, ui_action_scroll(u, times(kc)))
ACTION(k_up, ui_action_scroll(u, -times(kc)))
ACTION(k_first, ui_playlist_set_cursor(u->playlist,
                                       kc->has_count ? kc->count - 1 : 0))
ACTION(k_last, ui_playlist_set_cursor(u->playlist,
                                      kc->has_count
                                          ? kc->count - 1
                                          : (int)u->app->playlist.files.length))
ACTION(k_play, ui_action_play_selected(u, true))
ACTION(k_enqueue, ui_action_play_selected(u, false))
ACTION(k_gain_down, ui_action_adjust_gain(u, -GAIN_STEP_DB * (float)times(kc)))
ACTION(k_gain_up, ui_action_adjust_gain(u, GAIN_STEP_DB * (float)times(kc)))
ACTION(k_next, ui_action_next(u))
ACTION(k_prev, ui_action_prev(u))
ACTION(k_pause, ui_action_toggle_pause(u))
ACTION(k_page_up, ui_action_scroll(u, -half_page(u) * times(kc)))
ACTION(k_page_down, ui_action_scroll(u, half_page(u) * times(kc)))
ACTION(k_mute, ui_action_toggle_mute(u))
ACTION(k_wave, ui_action_cycle_spectrum_mode(u))
ACTION(k_seek_back, ui_action_seek_by(u, -SEEK_STEP_MS * times(kc)))
ACTION(k_seek_fwd, ui_action_seek_by(u, SEEK_STEP_MS * times(kc)))
ACTION(k_shuffle, ui_action_shuffle(u))
ACTION(k_sort, ui_action_cycle_sort(u))
ACTION(k_sort_dir, ui_action_toggle_sort_direction(u))
ACTION(k_redraw, tui_force_redraw(u->tui))
ACTION(k_cover, ui_cover_art_next_method(u->cover_art))
ACTION(k_center, ui_action_follow(u))
ACTION(k_lyrics_bigger, ui_action_lyrics_size(u, times(kc), false))
ACTION(k_lyrics_smaller, ui_action_lyrics_size(u, -times(kc), false))
ACTION(k_lyrics_reset, ui_action_lyrics_size(u, 0, true))
ACTION(k_cover_next, ui_action_cover_step(u, times(kc)))
ACTION(k_cover_prev, ui_action_cover_step(u, -times(kc)))
ACTION(k_search, ui_action_search_open(u))
ACTION(k_view_next, ui_view_select(u, (enum ui_view_id)((u->view + 1) % UI_VIEW_COUNT)))
ACTION(k_view_prev,
       ui_view_select(u, (enum ui_view_id)((u->view + UI_VIEW_COUNT - 1) % UI_VIEW_COUNT)))

static bool k_view(const tui_keyctx *kc, void *ud)
{
    ui_view_select(ud, (enum ui_view_id)(kc->keys[0].key - '1'));
    return true;
}

ACTION(s_accept, ui_action_search_accept(u))
ACTION(s_cancel, ui_action_search_cancel(u))
ACTION(s_next, ui_action_search_move(u, times(kc)))
ACTION(s_prev, ui_action_search_move(u, -times(kc)))
ACTION(s_page_down, ui_action_search_move(u, half_page(u)))
ACTION(s_page_up, ui_action_search_move(u, -half_page(u)))

void ui_hotkeys_init(ui_ctx *u)
{
    /* normal */
    u->km_normal = tui_keymap_new();
    int m = u->mode_normal = tui_keymap_mode(u->km_normal, "normal", 0);
    const tui_keybinding normal[] = {
        {"q", k_quit, u},          {"ZZ", k_quit, u},
        {"<C-c>", k_quit, u},      {"j", k_down, u},
        {"<Down>", k_down, u},     {"k", k_up, u},
        {"<Up>", k_up, u},         {"gg", k_first, u},
        {"<Home>", k_first, u},    {"G", k_last, u},
        {"<End>", k_last, u},      {"<CR>", k_play, u},
        {"<S-CR>", k_enqueue, u},  {"<C-j>", k_gain_down, u},
        {"<C-k>", k_gain_up, u},   {"N", k_next, u},
        {"P", k_prev, u},          {"<Space>", k_pause, u},
        {"{", k_page_up, u},       {"<C-u>", k_page_up, u},
        {"}", k_page_down, u},     {"<C-d>", k_page_down, u},
        {"m", k_mute, u},          {"<Left>", k_seek_back, u},
        {"<S-Left>", k_seek_back, u}, {"<Right>", k_seek_fwd, u},
        {"<S-Right>", k_seek_fwd, u}, {"r", k_shuffle, u},
        {"s", k_sort, u},          {"d", k_sort_dir, u},
        {"<C-l>", k_redraw, u},    {"<F1>", k_cover, u},
        {"zz", k_center, u},       {"/", k_search, u},
        {"w", k_wave, u},
        {"gt", k_view_next, u},    {"gT", k_view_prev, u},
        {"]", k_cover_next, u},    {"[", k_cover_prev, u},
        {"+", k_lyrics_bigger, u}, {"-", k_lyrics_smaller, u},
        {"=", k_lyrics_reset, u},
    };
    tui_keymap_bind_all(u->km_normal, m, normal, sizeof(normal) / sizeof(normal[0]));
    for (char c = '1'; c <= '9'; c++)
    {
        char key[2] = {c, 0};
        tui_keymap_bind(u->km_normal, m, key, k_view, u);
    }
    tui_router_set_global(u->router, u->km_normal, m);

    /* search: typing mode, so the langmap leaves the text alone */
    u->km_search = tui_keymap_new();
    int sm = u->mode_search = tui_keymap_mode(u->km_search, "search", TUI_MODE_TEXT);
    const tui_keybinding search[] = {
        {"<CR>", s_accept, u},     {"<Esc>", s_cancel, u},
        {"<C-c>", s_cancel, u},    {"<Down>", s_next, u},
        {"<C-n>", s_next, u},      {"<C-j>", s_next, u},
        {"<Tab>", s_next, u},      {"<Up>", s_prev, u},
        {"<C-p>", s_prev, u},      {"<C-k>", s_prev, u},
        {"<S-Tab>", s_prev, u},    {"<PageDown>", s_page_down, u},
        {"<PageUp>", s_page_up, u},
    };
    tui_keymap_bind_all(u->km_search, sm, search, sizeof(search) / sizeof(search[0]));
}

void ui_hotkeys_destroy(ui_ctx *u)
{
    tui_keymap_free(u->km_normal);
    tui_keymap_free(u->km_search);
    u->km_normal = u->km_search = NULL;
}
