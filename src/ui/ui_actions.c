#include "ui_internal.h"

#include "audio_mixer.h"
#include "audio_source.h"
#include "clock.h"
#include "playlist.h"
#include "utils.h"

/*
 * Everything the user can ask the player to do. Hotkeys and mouse widgets call
 * these and nothing else, so each behaviour exists exactly once and the input
 * layer stays a thin mapping.
 */

void ui_action_quit(ui_ctx *u)
{
    u->quit = true;
}

void ui_action_toggle_pause(ui_ctx *u)
{
    audio_mixer *m = &u->app->audio->mixer;
    m->paused = !m->paused;
}

void ui_action_toggle_mute(ui_ctx *u)
{
    audio_mixer *m = &u->app->audio->mixer;
    m->muted = !m->muted;
}

void ui_action_cycle_spectrum_mode(ui_ctx *u)
{
    u->spectrum_mode =
        (enum ui_spectrum_mode)((u->spectrum_mode + 1) % UI_SPECTRUM_MODE_COUNT);
}

void ui_action_adjust_gain(ui_ctx *u, float delta_db)
{
    u->app->audio->mixer.master_gain += delta_db;
}

void ui_action_next(ui_ctx *u)
{
    play_next(u->app);
    ui_playlist_follow_playing(u->playlist);
}

void ui_action_prev(ui_ctx *u)
{
    play_prev(u->app);
    ui_playlist_follow_playing(u->playlist);
}

void ui_action_play_selected(ui_ctx *u, bool clear_queue)
{
    uint64_t now = gclock_now_ns();
    if (now - u->last_play_ns < MS2NS(50))
        return;

    int idx = ui_playlist_cursor(u->playlist);
    if (idx < 0)
        return;
    play_at_index(u->app, idx, clear_queue);
    ui_playlist_follow_playing(u->playlist);
    u->last_play_ns = now;
}

void ui_action_center_cursor(ui_ctx *u)
{
    ui_playlist_recenter(u->playlist);
}

/* The list on screen moves: the playlist cursor, or the lyrics (which the
 * user takes over from the music by doing so). */
void ui_action_scroll(ui_ctx *u, int rows)
{
    switch (u->view)
    {
    case UI_VIEW_VISUAL:
        ui_lyrics_scroll(u->lyrics, rows);
        break;
    case UI_VIEW_METADATA:
        ui_lyrics_scroll(u->lyrics_full, rows);
        break;
    default:
        ui_playlist_move_cursor(u->playlist, rows);
        break;
    }
}

void ui_action_lyrics_size(ui_ctx *u, int delta, bool reset)
{
    int def = ui_options_default().lyrics.big_rows;
    int *rows = &u->opt.lyrics.big_rows;
    if (reset)
        *rows = def;
    else if (*rows <= 0)
        *rows = delta > 0 ? (def > 0 ? def : 4) : 0; /* off: + turns it on */
    else
        *rows = MATH_CLAMP(*rows + delta, UI_LYRICS_BIG_ROWS_MIN,
                           UI_LYRICS_BIG_ROWS_MAX);
    /* the lyrics widgets see the change on their next frame and rebuild */
}

/* Back to where the music is: the playing line, or the centre of the list. */
void ui_action_follow(ui_ctx *u)
{
    switch (u->view)
    {
    case UI_VIEW_VISUAL:
        ui_lyrics_follow(u->lyrics);
        break;
    case UI_VIEW_METADATA:
        ui_lyrics_follow(u->lyrics_full);
        break;
    default:
        ui_action_center_cursor(u);
        break;
    }
}

void ui_action_cover_step(ui_ctx *u, int dir)
{
    ui_cover_art_step(u->cover_art, dir);
}

void ui_action_search_open(ui_ctx *u)
{
    ui_search_open(u->search);
}

void ui_action_search_move(ui_ctx *u, int delta_rows)
{
    ui_playlist_move_cursor(u->playlist, delta_rows);
    ui_search_moved(u->search);
}

/* Esc: leave the search, the selection is where it was before. */
void ui_action_search_cancel(ui_ctx *u)
{
    ui_search_close(u->search, true, -1);
}

/* Enter: play what is selected and go back to the full list. */
void ui_action_search_accept(ui_ctx *u)
{
    int idx = ui_playlist_cursor(u->playlist);
    if (idx < 0)
        return; /* nothing matches: stay in the search */
    ui_search_close(u->search, false, idx);
    u->last_play_ns = 0; /* an explicit choice is never debounced away */
    ui_action_play_selected(u, true);
    ui_playlist_recenter(u->playlist);
}

void ui_action_seek_by(ui_ctx *u, int64_t delta_ms)
{
    audio_mixer *m = &u->app->audio->mixer;

    pthread_mutex_lock(&m->source_mutex);
    audio_source *src;
    ARR_FOREACH_BYREF(m->sources, src, i)
    {
        src->seek(src, delta_ms, SEEK_CUR);
    }
    pthread_mutex_unlock(&m->source_mutex);
}

/* `fraction` in [0, 1] of the current track; applied by the main loop. */
void ui_action_seek_to(ui_ctx *u, float fraction)
{
    audio_source *src = app_current_source(u->app);
    if (src == NULL || src->duration <= 0)
        return;

    u->app->want_to_seek_ms =
        (int64_t)((double)fraction * (double)src->duration / 1000.0);
}

void ui_action_shuffle(ui_ctx *u)
{
    playlist_shuffle(&u->app->playlist);
    ui_playlist_reordered(u->playlist);
}

void ui_action_cycle_sort(ui_ctx *u)
{
    playlist_manager *pl = &u->app->playlist;

    /* while shuffled, "next sort" re-sorts by the current key */
    enum playlist_sort next =
        pl->is_shuffled ? pl->sort : (pl->sort + 1) % PLAYLIST_SORT_LENGTH;
    playlist_sort(pl, next, pl->sort_direction);
    ui_playlist_reordered(u->playlist);
}

void ui_action_toggle_sort_direction(ui_ctx *u)
{
    playlist_manager *pl = &u->app->playlist;

    playlist_sort(pl, pl->sort, (pl->sort_direction + 1) % 2);
    ui_playlist_reordered(u->playlist);
}
