#ifndef __UI_FONT_H
#define __UI_FONT_H

/*
 * Text as pixels: a small wrapper around FreeType that draws UTF-8 text of any
 * size into a ui_fb (the sub-pixel framebuffer of ui_sprite.h), from where
 * ui_fb_to_cells() fits it to terminal cells. This is what lets a line of
 * lyrics be several rows tall, and be scaled while it grows.
 *
 * Glyphs are rendered the way a bitmap terminal font would be: hinted
 * (grid-fitted) and monochrome, at a whole number of pixels per em, placed on
 * whole pixels. At the 12 - 16 pixels an em has in a 3 - 4 row line this is
 * what keeps stems one or two pixels wide and the counters of e, a, 8 or 夢
 * open; an anti-aliased, unhinted rendering of the same outline turns to
 * mush. Fonts without hinting instructions go through FreeType's auto-hinter.
 * Variable fonts work: the weight axis is set to the requested weight (see
 * ui_font_set_weight()).
 *
 * A font is a list of files: the first one that loads is the primary, the
 * others are consulted for characters it lacks (a Latin font plus a CJK one,
 * say). Files are opened when first needed, and FreeType reads them from disk
 * as it needs them, so a fallback of tens of megabytes costs nothing until a
 * lyric actually uses it.
 *
 * Sizes are the height of the em square, in framebuffer sub-pixels (a cell is
 * UI_SUBCELL_W x UI_SUBCELL_H of them); they snap to whole sub-pixels, and
 * what is measured is what is drawn. Glyph bitmaps are cached per size, so
 * animating the size rasterizes each glyph once per step, not once per frame.
 *
 * Pure: no toolkit, no application state. FreeType is robust against broken
 * font files, but only open fonts the user chose or the system installed.
 */

#include "color.h"
#include "ui_sprite.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct ui_font ui_font;

/* Opens the first of `paths` that loads as the primary font and keeps the
 * rest, unread, as fallbacks. NULL when none of them is a usable font. */
ui_font *ui_font_open(const char *const *paths, int n);

/* A font for lyrics, from these files, in this order:
 *   1. `preferred` (a file; may be NULL or empty), then the file in the
 *      environment variable APLAYER_LYRICS_FONT;
 *   2. the fonts folder (.ttf .otf .ttc .otc, by name): the directory in the
 *      environment variable APLAYER_FONT_DIR, then `fonts` next to the
 *      executable, then `fonts` in the current directory;
 *   3. the bold sans and CJK fonts the platform usually ships;
 *   4. GNU Unifont, last, because it has a glyph for nearly everything and is
 *      exact at 16 pixels (a file called unifont* in the fonts folder, else
 *      where the system puts it).
 * NULL when there is not one usable file. */
ui_font *ui_font_open_default(const char *preferred);

void ui_font_close(ui_font *f);

/* The file of the primary font (the first that loads); valid as long as the
 * font is open. */
const char *ui_font_primary_path(const ui_font *f);

/* Weight, 1..1000 (400 regular, 700 bold), for variable fonts that have a
 * weight axis, clamped to the range the font has; 0 leaves the font at its
 * own default. Static fonts are not changed. Takes effect at once. */
void ui_font_set_weight(ui_font *f, int weight);

/* Distance from the baseline up to the top of the tallest glyphs, and down to
 * the bottom of the lowest, for an em of `px` (both positive, whole pixels). */
void ui_font_metrics(ui_font *f, float px, float *ascent, float *descent);

/* Width of `s` as drawn by ui_font_draw() at `px`. */
float ui_font_width(ui_font *f, const char *s, size_t len, float px);

typedef struct ui_font_span
{
    int off, len; /* bytes of the text */
} ui_font_span;

/* Greedy word wrap into lines of at most `max_w` sub-pixels. Words longer than
 * a line (and text without spaces, like Chinese) break wherever they have to;
 * every line holds at least one character, so the text always makes progress.
 * Empty text is one empty span. Spans never start or end with a space.
 *
 * Writes at most `cap` spans, the last one running on to the end of the text
 * if it did not all fit: `cap = len + 1` always suffices. Returns the count. */
int ui_font_wrap(ui_font *f, const char *s, size_t len, float px, float max_w,
                 ui_font_span *out, int cap);

/* Draws `s` with its baseline at `baseline` and its left edge at `x`
 * (sub-pixels; both are rounded to a whole pixel, glyphs are hinted for it).
 * Text is composited over what is in the framebuffer in `color`, `alpha`
 * (0..1) scaling its coverage. Characters no font has leave a gap of half an
 * em; invisible ones (variation selectors, joiners, zero-width spaces) take no
 * room at all. */
void ui_font_draw(ui_font *f, ui_fb *fb, const char *s, size_t len, float x,
                  float baseline, float px, color_t color, float alpha);

/* The same, but only the pixel columns from `clip_x0` up to (not including)
 * `clip_x1` are drawn (sub-pixels, as `x`; a glyph is cut where the clip
 * falls). Drawing a text twice, in two colours, with clips that meet, paints
 * it as a left-to-right fill. */
void ui_font_draw_clip(ui_font *f, ui_fb *fb, const char *s, size_t len,
                       float x, float baseline, float px, color_t color,
                       float alpha, float clip_x0, float clip_x1);

#endif /* __UI_FONT_H */
