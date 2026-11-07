#include "ui_theme.h"

#include "tui_widgets.h"

ui_theme ui_theme_default(void)
{
    const color_t primary = COLOR(230, 200, 150);
    const color_t panel = COLOR(10, 10, 10);
    const color_t surface = COLOR(30, 30, 30);

    return (ui_theme){
        .primary = primary,
        .primary_dim = COLOR(138, 120, 90),
        .text = COLOR(220, 220, 220),
        .control_bg = panel,

        .list =
            {
                   .normal_fg = COLOR_NONE,
                   .normal_bg = surface,
                   .playing_fg = surface,
                   .playing_bg = COLOR(91, 201, 77),
                   .number_fg = primary,
                   .number_bg = panel,
                   .match_fg = COLOR(255, 160, 60),
                   },
        .search = {.prompt_fg = primary,
                   .fg = COLOR(235, 235, 235),
                   .bg = surface,
                   .count_fg = COLOR(138, 120, 90)},
        .volume =
            {
                   .fg = primary,
                   .bg = panel,
                   .muted_fg = COLOR(245, 90, 60),
                   .muted_bg = panel,
                   },
        .timestamp = {.fg = primary, .bg = panel},
        .progress = {.fg = COLOR(255, 0, 0), .bg = surface},
        .statusline = {.fg = primary, .bg = panel},
        .media_control = {.fg = COLOR(0, 0, 0), .bg = COLOR(255, 255, 255)},

        .vu_meter =
            {
                   .fg = primary,
                   .bg = panel,
                   .sep = panel,
                   .level_fg = {[UI_LEVEL_LOW] = COLOR(0, 255, 0),
                             [UI_LEVEL_MID] = COLOR(255, 255, 0),
                             [UI_LEVEL_HIGH] = COLOR(255, 0, 0)},
                   .level_bg = {[UI_LEVEL_LOW] = COLOR(0, 100, 0),
                             [UI_LEVEL_MID] = COLOR(100, 100, 0),
                             [UI_LEVEL_HIGH] = COLOR(100, 0, 0)},
                   .act_off = COLOR_GRAY(0),
                   .act_on = COLOR_GRAY(255),
                   },

        .palette = ui_palette_from_accent(primary, COLOR(200, 160, 110)),
    };
}

ui_theme ui_theme_derive(const ui_theme *base, const ui_palette *pal)
{
    ui_theme t = *base;
    const color_t surface = pal->surface;
    const color_t dim = pal->text_dim;

    t.primary = pal->accent;
    t.primary_dim = dim;
    t.text = pal->text;
    t.control_bg = pal->bg;

    t.list.normal_fg = pal->text;
    t.list.normal_bg = pal->bg;
    t.list.playing_fg = pal->bg;
    t.list.playing_bg = pal->accent;
    t.list.number_fg = pal->accent;
    t.list.number_bg = pal->bg;
    t.list.match_fg = pal->hot;

    t.search.prompt_fg = pal->accent;
    t.search.fg = pal->text;
    t.search.bg = surface;
    t.search.count_fg = dim;

    t.volume.fg = pal->accent;
    t.volume.bg = COLOR_NONE;

    /* text on the page has no box of its own: the page shows */
    t.volume.muted_bg = COLOR_NONE;
    t.timestamp.fg = pal->accent;
    t.timestamp.bg = COLOR_NONE;
    t.progress.fg = pal->accent;
    t.progress.bg = surface;
    t.statusline.fg = pal->accent;
    t.statusline.bg = COLOR_NONE;
    t.media_control.fg = pal->bg;
    t.media_control.bg = pal->text;

    /* the page shows through the meter; its bars climb accent -> hot */
    t.vu_meter.fg = pal->accent;
    t.vu_meter.bg = COLOR_NONE;
    t.vu_meter.sep = pal->bg;
    t.vu_meter.level_fg[UI_LEVEL_LOW] = pal->accent;
    t.vu_meter.level_fg[UI_LEVEL_MID] = pal->accent2;
    t.vu_meter.level_fg[UI_LEVEL_HIGH] = pal->hot;
    for (int i = 0; i < UI_LEVEL_COUNT; i++)
        t.vu_meter.level_bg[i] =
            color_mix(pal->bg, t.vu_meter.level_fg[i], 0.22f);
    t.vu_meter.act_off = color_mix(pal->bg, pal->accent, 0.12f);
    t.vu_meter.act_on = pal->hot;

    t.palette = *pal;
    return t;
}

void ui_theme_apply_to_toolkit(const ui_theme *theme)
{
    tui_theme t = *tui_theme_get();
    t.accent = theme->primary;
    t.fg = theme->text;
    t.surface = theme->control_bg;
    t.track = theme->progress.bg;
    tui_theme_set(&t);
}
