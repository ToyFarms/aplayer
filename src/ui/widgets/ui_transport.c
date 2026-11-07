#include "tui.h"
#include "ui_internal.h"

#include "audio_mixer.h"
#include "audio_source.h"
#include "playlist.h"
#include "ui_format.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/*
 * The two control rows at the bottom of the screen:
 *
 *   0:42 / 3:15  ─────────── seek ───────────   V: +0.0 dB
 *   shuffle/sort           <<  [>  >>                A: -14.2 dB
 *
 * The compact one (VISUAL page) reads  0:42 [=== seek ===] 3:15  with a soft
 * drop shadow under the bar and under each button.
 *
 * Everything shown is a function of app state, refreshed by
 * ui_transport_sync() once per frame (setting an unchanged value is free).
 */

struct ui_transport
{
    ui_ctx *u;
    tui_node *root;
    tui_node *timestamp;
    tui_node *duration; /* compact only: the total, after the seek bar */
    tui_node *progress;
    tui_node *volume;
    tui_node *loudness;
    tui_node *statusline; /* NULL in the compact one */
    tui_node *prev_button, *play_button, *next_button;
    bool compact;
    int time_w; /* compact: columns of the two time labels */
};

static void transport_free(tui_node *n, void *ud)
{
    (void)n;
    free(ud);
}

static void on_prev(tui_node *n, void *ud)
{
    (void)n;
    ui_action_prev(((ui_transport *)ud)->u);
}

static void on_next(tui_node *n, void *ud)
{
    (void)n;
    ui_action_next(((ui_transport *)ud)->u);
}

static void on_play(tui_node *n, void *ud)
{
    (void)n;
    ui_action_toggle_pause(((ui_transport *)ud)->u);
}

static void on_seek(tui_node *n, float value, void *ud)
{
    (void)n;
    ui_action_seek_to(((ui_transport *)ud)->u, value);
}

/* A wrapper that casts a soft shadow down and to the left of what it holds:
 * the wrapper has one cell of padding at the bottom and at the left, and the
 * shadow is painted into it after the child is drawn. Half blocks keep it thin.
 * Its colour is the palette's own page colour made deeper, so it follows the
 * cover art instead of being black. */
static void shadow_paint(tui_node *n, tui_canvas *c, void *ud)
{
    (void)n;
    ui_ctx *u = ud;
    const ui_palette *pal = &u->theme.palette;
    color_t page = u->theme.control_bg;

    ui_oklch ok = ui_color_to_oklch(pal->bg);
    ok.L *= 0.2f;
    ok.C *= 1.15f;
    color_t shade = color_mix(ui_oklch_to_color(ok), pal->accent, 0.20f);

    int w = tui_canvas_width(c), h = tui_canvas_height(c);
    if (w < 2 || h < 2)
        return;
    for (int y = 0; y < h - 1; y++)
        tui_canvas_put(c, 0, y, 0x2590, (tui_pen){shade, page, 0}); /* left */
    for (int x = 1; x < w - 1; x++)
        tui_canvas_put(c, x, h - 1, 0x2584, (tui_pen){page, shade, 0}); /* below */
    tui_canvas_put(c, 0, h - 1, 0x259D, (tui_pen){shade, page, 0});
}

#define SHADOW_PAD ((tui_edges){0, 0, 1, 1}) /* top, right, bottom, left */

tui_node *ui_transport_root(const ui_transport *t)
{
    return t->root;
}

/* Two rows. The full one spans the screen; the compact one is as wide as its
 * parent makes it (the VISUAL page hangs it under the art), drops the sort
 * status, which belongs to the playlist, and adds a shadow row under each
 * of its two rows (four rows in all). */
ui_transport *ui_transport_new(ui_ctx *u, bool compact)
{
    ui_transport *t = calloc(1, sizeof(*t));
    if (t == NULL)
        abort();
    t->u = u;
    t->compact = compact;

    tui_ctx *ctx = u->tui;
    const ui_theme *th = &u->theme;
    const char *prev = compact ? " << " : "  <<  ";
    const char *play = compact ? " |> " : "  [>  ";
    const char *next = compact ? " >> " : "  >>  ";

    TUI_BOX(ctx, .dir = TUI_COLUMN, .shrink = TUI_NO_SHRINK,
            .on_destroy = transport_free, .userdata = t)
    {
        t->root = tui_cursor(ctx);

        /* row A: position, seek bar, (master gain). The compact one runs
         * [position][seek bar][duration] and keeps a row for the bar's
         * shadow. */
        TUI_BOX(ctx, .height = TUI_CELLS(compact ? 2 : 1),
                .align_items = compact ? TUI_ALIGN_START : TUI_ALIGN_AUTO,
                .margin = {compact ? 0 : 1, 0, 0, 0},
                .padding = {0, 0, 0, compact ? 0 : 2})
        {
            TUI_TEXT(ctx, compact ? "0:00" : "0:00 / 0:00",
                     .shrink = TUI_NO_SHRINK,
                     .fg = th->timestamp.fg, .bg = th->timestamp.bg);
            t->timestamp = tui_last(ctx);

            if (compact)
            {
                TUI_BOX(ctx, .grow = 1, .margin = {0, 2, 0, 1},
                        .padding = SHADOW_PAD, .paint_over = shadow_paint,
                        .userdata = u)
                {
                    t->progress = tui_slider(ctx, 0.0f, on_seek, t,
                                             &(tui_attr){._init = 0});
                }
            }
            else
            {
                t->progress = tui_slider(ctx, 0.0f, on_seek, t,
                                         &(tui_attr){
                                             ._init = 0, .margin = {0, 0, 0, 1}
                });
            }
            tui_progress_set_colors(t->progress, th->progress.fg,
                                    th->progress.bg);
            ui_mouse_only(t->progress);

            if (compact)
            {
                TUI_TEXT(ctx, "0:00", .shrink = TUI_NO_SHRINK,
                         .text_align = TUI_TEXT_RIGHT, .fg = th->timestamp.fg,
                         .bg = th->timestamp.bg);
                t->duration = tui_last(ctx);
            }
            else
            {
                TUI_TEXT(ctx, "", .width = TUI_CELLS(13),
                         .shrink = TUI_NO_SHRINK, .margin = {0, 1, 0, 1},
                         .fg = th->volume.fg, .bg = th->volume.bg);
                t->volume = tui_last(ctx);
            }
        }

        /* row B: sort state, transport buttons, loudness */
        TUI_BOX(ctx, .height = TUI_CELLS(compact ? 2 : 1),
                .align_items = compact ? TUI_ALIGN_START : TUI_ALIGN_AUTO,
                .padding = {0, 0, 0, compact ? 0 : 2},
                .justify = compact ? TUI_JUSTIFY_BETWEEN : TUI_JUSTIFY_START)
        {
            if (compact)
            {
                TUI_TEXT(ctx, "", .width = TUI_CELLS(10),
                         .shrink = TUI_NO_SHRINK, .fg = th->volume.fg,
                         .bg = th->volume.bg);
                t->volume = tui_last(ctx);
            }
            else
            {
                TUI_TEXT(ctx, "", .grow = 1, .fg = th->statusline.fg,
                         .bg = th->statusline.bg);
                t->statusline = tui_last(ctx);
            }

            TUI_BOX(ctx, .shrink = TUI_NO_SHRINK,
                    .margin = {0, compact ? 0 : 2, 0, compact ? 0 : 2},
                    .col_gap = compact ? 2 : 2)
            {
                tui_attr bare = {
                    ._init = 0, .padding = {0, 0, 0, 0}
                };
                tui_node **slot[] = {&t->prev_button, &t->play_button,
                                     &t->next_button};
                const char *label[] = {prev, play, next};
                tui_click_fn click[] = {on_prev, on_play, on_next};
                for (int i = 0; i < 3; i++)
                {
                    if (compact)
                    {
                        /* the button, with its shadow down and left */
                        TUI_BOX(ctx, .shrink = TUI_NO_SHRINK,
                                .padding = SHADOW_PAD,
                                .paint_over = shadow_paint, .userdata = u)
                        {
                            *slot[i] = tui_button(ctx, label[i], click[i], t,
                                                  &bare);
                        }
                    }
                    else
                        *slot[i] = tui_button(ctx, label[i], click[i], t,
                                              &bare);
                    ui_mouse_only(*slot[i]);
                }
            }

            if (!compact)
                TUI_SPACER(ctx);
            TUI_TEXT(ctx, "", .width = TUI_CELLS(compact ? 10 : 13),
                     .shrink = TUI_NO_SHRINK,
                     .margin = {0, compact ? 0 : 1, 0, 0},
                     .text_align = compact ? TUI_TEXT_RIGHT : TUI_TEXT_LEFT,
                     .fg = th->primary_dim, .bg = th->volume.bg);
            t->loudness = tui_last(ctx);
        }
    }

    ui_transport_restyle(t);
    return t;
}

void ui_transport_restyle(ui_transport *t)
{
    const ui_theme *th = &t->u->theme;

    tui_set_fg(t->timestamp, th->timestamp.fg);
    tui_set_bg(t->timestamp, th->timestamp.bg);
    if (t->duration)
    {
        tui_set_fg(t->duration, th->timestamp.fg);
        tui_set_bg(t->duration, th->timestamp.bg);
    }
    tui_progress_set_colors(t->progress, th->progress.fg, th->progress.bg);
    if (t->statusline)
    {
        tui_set_fg(t->statusline, th->statusline.fg);
        tui_set_bg(t->statusline, th->statusline.bg);
    }
    tui_set_fg(t->timestamp, th->timestamp.fg);
    tui_set_bg(t->timestamp, th->timestamp.bg);
    tui_set_fg(t->volume, th->volume.fg);
    tui_set_bg(t->volume, th->volume.bg);
    tui_set_fg(t->loudness, th->primary_dim);
    tui_set_bg(t->loudness, th->volume.bg);

    tui_node *buttons[] = {t->prev_button, t->play_button, t->next_button};
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++)
        tui_button_set_style(buttons[i],
                             t->compact ? th->search.bg : th->control_bg,
                             th->primary,
                             th->media_control.bg, th->media_control.fg);
}

void ui_transport_sync(ui_transport *t)
{
    ui_ctx *u = t->u;
    const ui_theme *th = &u->theme;
    app_instance *app = u->app;
    audio_mixer *mixer = &app->audio->mixer;
    audio_source *src = app_current_source(app);
    char buf[96], a[32], b[32];

    if (src != NULL)
    {
        ui_format_time(a, sizeof(a), (uint64_t)src->timestamp);
        ui_format_time(b, sizeof(b), (uint64_t)src->duration);
        if (t->compact)
        {
            /* [position][seek bar][duration]: both ends as wide as the
             * longer one, so the bar does not jitter as digits roll over */
            int w = (int)strlen(a);
            if ((int)strlen(b) > w)
                w = (int)strlen(b);
            tui_set_text(t->timestamp, a);
            tui_set_text(t->duration, b);
            if (t->time_w != w)
            {
                t->time_w = w;
                tui_set_size(t->timestamp, TUI_CELLS(w), TUI_CELLS(1));
                tui_set_size(t->duration, TUI_CELLS(w), TUI_CELLS(1));
            }
        }
        else
        {
            snprintf(buf, sizeof(buf), "%s / %s", a, b);
            tui_set_text(t->timestamp, buf);
        }

        double p = src->duration > 0
                       ? (double)src->timestamp / (double)src->duration
                       : 0.0;
        tui_progress_set(t->progress, isnan(p) ? 1.0f : (float)p);
    }

    /* master gain: strike-through + warning colours while muted */
    {
        bool muted = mixer->muted;
        float g = mixer->master_gain;
        snprintf(buf, sizeof(buf), t->compact ? "V %s%.1fdB" : "V: %s%.1f dB",
                 g > 0.0f ? "+" : "", g);
        tui_set_text(t->volume, buf);

        tui_attr at = tui_get_attr(t->volume);
        at.fg = muted ? th->volume.muted_fg : th->volume.fg;
        at.bg = muted ? th->volume.muted_bg : th->volume.bg;
        if (muted)
            at.attrs |= TUI_STRIKE;
        else
            at.attrs &= (uint16_t)~TUI_STRIKE;
        tui_set_attr(t->volume, &at);
    }

    if (src != NULL && src->get_loudness)
    {
        snprintf(buf, sizeof(buf), t->compact ? "A %.1fdB" : "A: %.1f dB",
                 src->get_loudness(src));
        tui_set_text(t->loudness, buf);
    }
    else
        tui_set_text(t->loudness, "");

    bool stopped = mixer->paused || mixer->sources.length == 0;
    tui_button_set_label(t->play_button, t->compact ? (stopped ? " [> " : " [] ")
                                                    : (stopped ? "  [>  " : "  []  "));

    if (t->statusline)
    {
        snprintf(buf, sizeof(buf), "%s%s/%s",
                 app->playlist.is_shuffled ? "*" : "",
                 playlist_sort_name(app->playlist.sort),
                 playlist_sort_dir_name(app->playlist.sort_direction));
        tui_set_text(t->statusline, buf);
    }
}
