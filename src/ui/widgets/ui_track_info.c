#include "ui_internal.h"

#include "audio_source.h"
#include "metagen.h"

#include <stdio.h>
#include <string.h>

/*
 * What is playing, as four short lines:
 *
 *   Title                              (bold, warms up on a peak)
 *   Artist, Artist
 *   Album · 2019
 *   Genre · 44.1 kHz · 320 kbps · stereo
 *
 * Lines without content stay empty. The text comes from the tags; a file
 * without any is shown under its file name.
 */

#define LINE_MAX 384

typedef struct track_info
{
    ui_ctx *u;
    tui_node *title, *artist, *album, *tech;

    const void *meta; /* what the texts were built from */
    uint32_t serial;
    int level; /* energy level the title colour was set for */
    uint32_t theme_rev;
} track_info;

static void info_free(tui_node *n, void *ud)
{
    (void)n;
    free(ud);
}

/* appends `part` to `out` with `sep` in between unless `out` is still empty */
static void join(char *out, size_t cap, const char *sep, const char *part)
{
    if (part == NULL || part[0] == '\0')
        return;
    size_t len = strlen(out);
    snprintf(out + len, cap - len, "%s%s", len ? sep : "", part);
}

static void describe(track_info *ti, const metadata_t *m)
{
    char title[LINE_MAX] = "", artist[LINE_MAX] = "", album[LINE_MAX] = "";
    char tech[LINE_MAX] = "", tmp[64];

    if (m != NULL)
    {
        join(title, sizeof(title), "", m->title);
        for (size_t i = 0; i < m->n_artists; i++)
            join(artist, sizeof(artist), ", ", m->artists[i]);
        if (artist[0] == '\0')
            join(artist, sizeof(artist), "", m->album_artist);
        join(album, sizeof(album), "", m->album);
        if (m->recording_time && strlen(m->recording_time) >= 4)
        {
            snprintf(tmp, sizeof(tmp), "%.4s", m->recording_time);
            join(album, sizeof(album), " \u00b7 ", tmp);
        }

        join(tech, sizeof(tech), " \u00b7 ", m->genre);
        if (m->sample_rate_hz > 0)
        {
            snprintf(tmp, sizeof(tmp), "%.1f kHz",
                     (double)m->sample_rate_hz / 1000.0);
            join(tech, sizeof(tech), " \u00b7 ", tmp);
        }
        if (m->bitrate_kbps > 0)
        {
            snprintf(tmp, sizeof(tmp), "%d kbps", m->bitrate_kbps);
            join(tech, sizeof(tech), " \u00b7 ", tmp);
        }
        if (m->channels > 0)
        {
            if (m->channels <= 2)
                snprintf(tmp, sizeof(tmp), "%s",
                         m->channels == 1 ? "mono" : "stereo");
            else
                snprintf(tmp, sizeof(tmp), "%d ch", m->channels);
            join(tech, sizeof(tech), " \u00b7 ", tmp);
        }
    }

    if (title[0] == '\0')
    {
        const fs_entry_t *file = ti->u->app->playlist.current_file;
        if (file != NULL)
            snprintf(title, sizeof(title), "%.*s", (int)file->name.len,
                     file->name.buf);
    }

    tui_set_text(ti->title, title);
    tui_set_text(ti->artist, artist);
    tui_set_text(ti->album, album);
    tui_set_text(ti->tech, tech);
}

static void info_update(tui_node *n, double dt, void *ud)
{
    (void)n;
    (void)dt;
    track_info *ti = ud;
    ui_ctx *u = ti->u;
    audio_source *src = app_current_source(u->app);
    const metadata_t *m =
        src && src->get_metadata ? src->get_metadata(src) : NULL;

    if (ti->meta != m || ti->serial != u->app->track_serial)
    {
        ti->meta = m;
        ti->serial = u->app->track_serial;
        describe(ti, m);
    }

    /* the title warms towards the highlight on a peak */
    ui_ambience_params ap = ui_ambience_params_of(&u->opt);
    bool follow = u->skin_active;
    int level = follow ? u->ambience.level : 0;
    if (level == ti->level && ti->theme_rev == u->ambience.rev)
        return;
    ti->level = level;
    ti->theme_rev = u->ambience.rev;

    const ui_theme *t = &u->theme;
    float pulse =
        follow ? ui_ambience_pulse(&u->ambience, &ap) * ap.depth : 0.0f;
    tui_set_fg(ti->title, color_mix(t->text, t->palette.hot, pulse * 0.6f));
    tui_set_fg(ti->artist, t->primary);
    tui_set_fg(ti->album, t->palette.text_dim);
    tui_set_fg(ti->tech, t->palette.text_dim);
}

tui_node *ui_track_info_new(ui_ctx *u)
{
    track_info *ti = calloc(1, sizeof(*ti));
    if (ti == NULL)
        abort();
    ti->u = u;
    ti->level = -1;

    tui_ctx *ctx = u->tui;
    tui_node *root = NULL;
    TUI_BOX(ctx, .dir = TUI_COLUMN, .height = TUI_CELLS(4), .width = TUI_CELLS(40),
            .shrink = TUI_NO_SHRINK, .on_update = info_update,
            .on_destroy = info_free, .userdata = ti)
    {
        root = tui_cursor(ctx);
        TUI_TEXT(ctx, "", .text_wrap = TUI_TEXT_ELLIPSIS, .attrs = TUI_BOLD,
                 .text_align = TUI_TEXT_CENTER);
        ti->title = tui_last(ctx);
        TUI_TEXT(ctx, "", .text_wrap = TUI_TEXT_ELLIPSIS,
                 .text_align = TUI_TEXT_CENTER);
        ti->artist = tui_last(ctx);
        TUI_TEXT(ctx, "", .text_wrap = TUI_TEXT_ELLIPSIS,
                 .text_align = TUI_TEXT_CENTER);
        ti->album = tui_last(ctx);
        TUI_TEXT(ctx, "", .text_wrap = TUI_TEXT_ELLIPSIS,
                 .text_align = TUI_TEXT_CENTER);
        ti->tech = tui_last(ctx);
    }
    return root;
}
