#ifndef __UI_THEME_H
#define __UI_THEME_H

/*
 * Every colour the interface uses, as named, typed fields: a misspelt colour
 * is a compile error rather than a silent lookup miss at runtime. COLOR_NONE
 * means "leave the terminal's default".
 */

#include "color.h"
#include "ui_palette.h"

enum ui_level
{
    UI_LEVEL_LOW,
    UI_LEVEL_MID,
    UI_LEVEL_HIGH,
    UI_LEVEL_COUNT,
};

typedef struct ui_theme
{
    color_t primary;
    color_t primary_dim;
    color_t text;
    color_t control_bg;

    struct
    {
        color_t normal_fg, normal_bg;
        color_t playing_fg, playing_bg;
        color_t number_fg, number_bg;
        color_t match_fg; /* characters matched by the search query */
    } list;

    /* the / search bar */
    struct
    {
        color_t prompt_fg, fg, bg, count_fg;
    } search;

    struct
    {
        color_t fg, bg;
        color_t muted_fg, muted_bg;
    } volume;

    struct
    {
        color_t fg, bg;
    } timestamp, progress, statusline;

    /* Hover colours of the transport buttons. */
    struct
    {
        color_t fg, bg;
    } media_control;

    struct
    {
        color_t fg, bg, sep;
        color_t level_fg[UI_LEVEL_COUNT];
        color_t level_bg[UI_LEVEL_COUNT];
        color_t act_off, act_on; /* the ACT row, silent .. fully active */
    } vu_meter;

    /* What the VISUAL page wears when the cover art gives it nothing to
     * follow (no art, art not loaded yet). */
    ui_palette palette;
} ui_theme;

ui_theme ui_theme_default(void);

/* `base` recoloured with `pal`: every field is remapped, so any widget that
 * reads the theme follows the cover art without knowing about it. Semantic
 * colours (muted) are kept. */
ui_theme ui_theme_derive(const ui_theme *base, const ui_palette *pal);

/* Pushes the theme's base colours into the toolkit's global widget theme
 * (buttons, sliders, ...). Call before building any widgets. */
void ui_theme_apply_to_toolkit(const ui_theme *theme);

#endif /* __UI_THEME_H */
