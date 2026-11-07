#ifndef __UI_INTERNAL_H
#define __UI_INTERNAL_H

/*
 * Private to src/ui. Nothing outside this directory includes this header; the
 * rest of the program talks to the UI through include/ui.h only.
 *
 * Layering inside the module:
 *
 *   ui.c            lifecycle, event routing, frame loop
 *   ui_views.c      what each tab shows, and switching between them
 *   ui_hotkeys.c    global keymap  -+
 *   ui_actions.c    user intents   -+-> the only code that changes playback
 *   widgets/        self-contained components (own their state, read app state)
 *   ui_theme/options/format/ansi    data and pure helpers, no widget knowledge
 *
 * Widgets never reach into each other. Anything shared goes through ui_ctx.
 */

#include "_math.h"
#include "app.h"
#include "color.h"
#include "tui.h"
#include "fuzzy.h"
#include "tui_router.h"
#include "tui_term.h"
#include "tui_widgets.h"
#include "ui.h"
#include "ui_ambience.h"
#include "ui_font.h"
#include "ui_options.h"
#include "ui_theme.h"

#include <stdint.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------ */
/* views (tabs)                                                             */
/* ------------------------------------------------------------------------ */

/* To add a view: add an id here, a row to the table in ui_views.c. */
enum ui_view_id
{
    UI_VIEW_PLAYLIST,
    UI_VIEW_VISUAL,
    UI_VIEW_METADATA,
    UI_VIEW_COUNT,
};

/* ------------------------------------------------------------------------ */
/* context                                                                  */
/* ------------------------------------------------------------------------ */

typedef struct ui_tab_bar ui_tab_bar;
typedef struct ui_transport ui_transport;
typedef struct ui_playlist ui_playlist;
typedef struct ui_cover_art ui_cover_art;
typedef struct ui_search ui_search;
typedef struct ui_lyrics ui_lyrics;

struct ui_ctx
{
    app_instance *app;
    tui_ctx *tui;
    tui_term_input input;
    tui_router *router;
    tui_keymap *km_normal, *km_search;
    int mode_normal, mode_search;

    ui_theme theme;      /* what widgets read: `base_theme`, or the cover's */
    ui_theme base_theme; /* the configured one */
    ui_options opt;

    /* The VISUAL page follows the cover art: while it is shown `theme` is
     * derived from `ambience.now` (see ui_skin.c). */
    ui_ambience ambience;
    bool skin_active;
    uint32_t skin_cover_rev; /* cover the ambience is fading towards */
    uint32_t skin_rev;       /* palette revision `theme` was derived from */
    bool skin_fading;        /* a palette fade was running at the last sync */

    enum ui_view_id view;
    tui_node *view_root[UI_VIEW_COUNT];

    /* Components that hotkeys / actions need to address. Owned by their
     * toolkit node; valid from build until ui_destroy(). */
    ui_tab_bar *tab_bar;
    ui_transport *transport;        /* full width, every view but VISUAL */
    ui_transport *transport_visual; /* under the art on the VISUAL page */
    bool color256;                  /* terminal has no truecolor */
    ui_playlist *playlist;
    ui_cover_art *cover_art;
    ui_search *search;
    ui_lyrics *lyrics;      /* lyrics of the VISUAL page */
    ui_lyrics *lyrics_full; /* lyrics of the METADATA page */
    enum ui_spectrum_mode spectrum_mode; /* what ui_spectrum draws now */

    ui_font *font;   /* for big text; opened on first use, see ui_ctx_font() */
    bool font_tried; /* ... so a machine without one looks once, not per frame */

    uint64_t last_frame_ns;
    uint64_t last_play_ns; /* debounce for "play selected" */
    bool quit;
};

static inline ui_ambience_params ui_ambience_params_of(const ui_options *o)
{
    return (ui_ambience_params){.levels = o->ambience.levels,
                                .pulse_hz = o->ambience.pulse_hz,
                                .release_ms = o->ambience.release_ms,
                                .fade_ms = o->ambience.fade_ms,
                                .depth = o->ambience.depth};
}

/* Buttons and sliders are mouse controls: if they could take keyboard focus,
 * Space / Enter / arrows would go to whichever one was clicked last instead of
 * the global keymap. */
static inline void ui_mouse_only(tui_node *n)
{
    tui_attr a = tui_get_attr(n);
    a.focusable = false;
    tui_set_attr(n, &a);
}

static inline tui_pen ui_pen(color_t fg, color_t bg)
{
    return (tui_pen){fg, bg, 0};
}

/* The font big text is drawn in, shared by every widget that needs it. Opened
 * (and its first file read) on the first call; NULL if the machine has none,
 * in which case widgets keep to terminal text. */
ui_font *ui_ctx_font(ui_ctx *u);

/* ------------------------------------------------------------------------ */
/* views                                                                    */
/* ------------------------------------------------------------------------ */

const char *ui_view_title(enum ui_view_id id);
/* Builds every view under the current build cursor. */
void ui_views_build(ui_ctx *u);
/* Shows `id`, hides the rest, updates the tab bar. */
void ui_view_select(ui_ctx *u, enum ui_view_id id);

/* ------------------------------------------------------------------------ */
/* actions: what the user can ask for (shared by hotkeys and mouse)         */
/* ------------------------------------------------------------------------ */

void ui_action_quit(ui_ctx *u);
void ui_action_toggle_pause(ui_ctx *u);
void ui_action_toggle_mute(ui_ctx *u);
/* Spectrum line <-> oscilloscope. */
void ui_action_cycle_spectrum_mode(ui_ctx *u);
void ui_action_adjust_gain(ui_ctx *u, float delta_db);
void ui_action_next(ui_ctx *u);
void ui_action_prev(ui_ctx *u);
void ui_action_play_selected(ui_ctx *u, bool clear_queue);
void ui_action_seek_by(ui_ctx *u, int64_t delta_ms);
void ui_action_seek_to(ui_ctx *u, float fraction);
void ui_action_shuffle(ui_ctx *u);
void ui_action_cycle_sort(ui_ctx *u);
void ui_action_toggle_sort_direction(ui_ctx *u);
void ui_action_center_cursor(ui_ctx *u);
void ui_action_scroll(ui_ctx *u, int rows); /* playlist cursor / lyrics */
void ui_action_follow(ui_ctx *u);           /* lyrics: back to auto scroll */
/* Size of the big lyric line, in rows: `delta` more or fewer (within
 * UI_LYRICS_BIG_ROWS_MIN..MAX), or, with `reset`, the default again. */
void ui_action_lyrics_size(ui_ctx *u, int delta, bool reset);
void ui_action_cover_step(ui_ctx *u, int dir); /* next (+1) / previous cover */
void ui_action_search_open(ui_ctx *u);   /* "/" */
void ui_action_search_accept(ui_ctx *u); /* Enter: play the selection, back to the full list */
void ui_action_search_cancel(ui_ctx *u); /* Esc: back to the full list, cursor restored */
void ui_action_search_move(ui_ctx *u, int delta_rows);

/* Builds the keymaps (normal + search) and installs the router's global map. */
void ui_hotkeys_init(ui_ctx *u);
void ui_hotkeys_destroy(ui_ctx *u);

/* ------------------------------------------------------------------------ */
/* widgets (src/ui/widgets)                                                 */
/* ------------------------------------------------------------------------ */

/* Constructors create their node under the current build cursor. */

ui_tab_bar *ui_tab_bar_new(ui_ctx *u);
void ui_tab_bar_select(ui_tab_bar *bar, enum ui_view_id id);

/* Timestamp, seek bar, volume, status line, transport buttons. */
ui_transport *ui_transport_new(ui_ctx *u, bool compact);
tui_node *ui_transport_root(const ui_transport *t);
void ui_transport_sync(ui_transport *t); /* call once per frame */
/* Re-applies the theme's colours (it changed under the widget). */
void ui_transport_restyle(ui_transport *t);

/* What the playlist shows while a search is active: row r is the track at
 * playlist index hits[r].id. Owned by the search; the playlist only reads it. */
typedef struct ui_filter
{
    const fuzzy_hit *hits;
    int count;
    uint32_t rev; /* bump when hits/count changed */
    const fuzzy_query *query; /* for highlighting */
} ui_filter;

ui_playlist *ui_playlist_new(ui_ctx *u);
/* Playlist index of the selected row (-1 if there is none). */
int ui_playlist_cursor(const ui_playlist *p);
/* Rows are playlist indices unless a filter is set. */
void ui_playlist_set_cursor(ui_playlist *p, int row); /* clamped */
void ui_playlist_set_filter(ui_playlist *p, const ui_filter *f, bool reset_cursor);
/* Back to the full list, cursor on playlist index `select` (-1: unchanged). */
void ui_playlist_clear_filter(ui_playlist *p, int select);
bool ui_playlist_filtered(const ui_playlist *p);
int ui_playlist_cursor_row(const ui_playlist *p);
void ui_playlist_recenter(ui_playlist *p);
void ui_playlist_move_cursor(ui_playlist *p, int delta);
/* Moves the cursor onto the playing track if a new track started since the
 * last call. Also done automatically every frame. */
void ui_playlist_follow_playing(ui_playlist *p);
/* The playlist order changed: cursor goes to the playing track, view recentres
 * and repaints. */
void ui_playlist_reordered(ui_playlist *p);

/* The / bar. Hidden until opened; builds under the current build cursor. */
ui_search *ui_search_new(ui_ctx *u);
void ui_search_open(ui_search *s);
/* Closes the bar. `restore`: put the cursor back where it was (cancel);
 * otherwise it goes to playlist index `select` (accept). */
void ui_search_close(ui_search *s, bool restore, int select);
bool ui_search_active(const ui_search *s);
void ui_search_moved(ui_search *s); /* user moved the selection */
void ui_search_sync(ui_search *s);  /* once per frame: continue a long scan */

tui_node *ui_vu_meter_new(ui_ctx *u);

/* A few rows of braille, in one of three modes (`u->spectrum_mode`):
 *  - LINE_FILL / LINE: the spectrum of what is playing as one line, low
 *    frequencies on the left, drawn the way a DAW's analyzer does, with or
 *    without the area under it filled;
 *  - SCOPE: its waveform, the newest samples at the right edge, scrolling
 *    left like an oscilloscope. */
tui_node *ui_spectrum_new(ui_ctx *u, int rows);

/* `height` in cells, 0 = fill the available space. */
ui_lyrics *ui_lyrics_new(ui_ctx *u, int height);
/* The user takes over: move the text by `rows` (>0 towards the end). Following
 * the music resumes with ui_lyrics_follow(). */
void ui_lyrics_scroll(ui_lyrics *l, int rows);
void ui_lyrics_follow(ui_lyrics *l);
tui_node *ui_lyrics_node(const ui_lyrics *l);

ui_cover_art *ui_cover_art_new(ui_ctx *u);
void ui_cover_art_next_method(ui_cover_art *a);
/* Brings the next (dir > 0) or previous picture to the front. */
void ui_cover_art_step(ui_cover_art *a, int dir);
/* Palette of the picture in front. `rev` changes whenever it does (a new
 * track, another picture); false while there is no picture. */
bool ui_cover_art_palette(const ui_cover_art *a, ui_palette *out,
                          uint32_t *rev);

/* Title, artists, album and technical details of the playing track. */
tui_node *ui_track_info_new(ui_ctx *u);

/* ------------------------------------------------------------------------ */
/* skin: the live theme of the VISUAL page                                  */
/* ------------------------------------------------------------------------ */

/* Starts / stops following the cover art according to the view on screen.
 * Called by ui_view_select(). */
void ui_skin_apply(ui_ctx *u);
/* Once per frame, before tui_update(): feeds the ambience, re-derives the
 * theme while it fades. Does nothing while another view is shown. */
void ui_skin_sync(ui_ctx *u, double dt);

#endif /* __UI_INTERNAL_H */
