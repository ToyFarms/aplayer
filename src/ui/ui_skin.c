#include "ui_internal.h"

#include "audio_mixer.h"

#include <string.h>

/*
 * The VISUAL page follows the cover art: while it is on screen the theme the
 * widgets read is derived from the ambience's current palette, which fades to
 * the next cover's colours whenever the picture in front changes. Leaving the
 * page restores the configured theme, so the other views never notice.
 *
 * Only palette changes touch the theme. The music's energy is read by the
 * widgets that show it (art, title, lyrics) straight from `ui_ctx.ambience`,
 * so a beat repaints those and not the whole interface.
 */

/* `redraw`: also resend the whole screen (the page appears / disappears, or a
 * fade has just ended so nothing paused is left in a stale colour) */
static void apply_theme(ui_ctx *u, bool redraw)
{
    u->theme = u->skin_active
                   ? ui_theme_derive(&u->base_theme, &u->ambience.now)
                   : u->base_theme;
    u->skin_rev = u->ambience.palette_rev;

    /* the application's own bars sit on the page, so the page's colour is
     * the screen's */
    tui_set_bg(tui_root(u->tui),
               u->skin_active ? u->theme.control_bg : COLOR_NONE);
    ui_theme_apply_to_toolkit(&u->theme);
    ui_transport_restyle(u->transport);
    ui_transport_restyle(u->transport_visual);
    ui_tab_bar_select(u->tab_bar, u->view);
    if (redraw)
        tui_force_redraw(u->tui);
}

void ui_skin_apply(ui_ctx *u)
{
    bool want = u->view == UI_VIEW_VISUAL;
    if (want == u->skin_active)
        return;
    u->skin_active = want;
    apply_theme(u, true);
}

static float loudest_activity(const app_level_meter *m)
{
    float peak = 0.0f;
    for (int i = 0; i < m->channels && i < APP_METERS_MAX_CHANNELS; i++)
        if (m->level[i] > peak)
            peak = m->level[i];
    return peak;
}

void ui_skin_sync(ui_ctx *u, double dt)
{
    if (!u->skin_active)
        return;
    app_instance *app = u->app;

    ui_palette pal;
    uint32_t rev;
    bool has_art = ui_cover_art_palette(u->cover_art, &pal, &rev);
    if (rev != u->skin_cover_rev)
    {
        u->skin_cover_rev = rev;
        ui_ambience_target(&u->ambience,
                           has_art ? &pal : &u->base_theme.palette);
    }

    /* paused: the analyzers stop reporting, their last value must not stick */
    bool silent = app->audio->mixer.paused || app_current_source(app) == NULL;
    ui_ambience_params ap = ui_ambience_params_of(&u->opt);
    ui_ambience_step(&u->ambience, dt, loudest_activity(&app->meters.activity),
                     silent, &ap);

    /* the theme follows the palette fade; the screen is resent only when a
     * fade has just ended */
    bool fading = u->ambience.fade < 1.0f;
    if (u->ambience.palette_rev != u->skin_rev)
        apply_theme(u, u->skin_fading && !fading);
    u->skin_fading = fading;
}
