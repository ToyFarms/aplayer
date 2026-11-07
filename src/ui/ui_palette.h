#ifndef __UI_PALETTE_H
#define __UI_PALETTE_H

/*
 * The colour scheme the interface wears while it follows the cover art. Pure
 * data and pure functions (no widget, no toolkit), so the generator can be
 * unit tested on synthetic pictures.
 *
 * The palette is *designed*, not sampled. The art only decides a few numbers:
 * the dominant hue, optionally a second hue, how vivid the picture is and how
 * bright. Everything else comes from colour theory, worked in OKLCH (equal
 * steps in lightness / chroma look equal):
 *
 *   - the page is a mid-dark tint of the dominant hue (never black unless the
 *     art is), with a slightly lighter `surface`, a `glow` that leans towards
 *     the second hue, and a `pulse` colour the whole page breathes towards;
 *   - `accent` is the dominant hue made light and clean; `accent2` sits on a
 *     harmonic angle from it (analogous, split-complementary or complementary,
 *     whichever is nearest the art's second hue; analogous when there is none);
 *   - `hot` is a very light tint of accent2, `text` / `text_dim` a nearly
 *     neutral tint of the dominant hue, both with plenty of contrast on `bg`.
 */

#include "color.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct ui_palette
{
    color_t bg;       /* page base */
    color_t surface;  /* panels, tracks, the search bar */
    color_t glow;     /* unused since the backdrop was removed */
    color_t pulse;    /* what the whole page brightens towards on the beat */
    color_t accent;   /* primary highlight (progress, current line, bars) */
    color_t accent2;  /* second hue, a harmonic of the first */
    color_t hot;      /* lightest highlight (peaks, beat flashes) */
    color_t text;     /* body text on `bg` */
    color_t text_dim; /* secondary text on `bg` */
} ui_palette;

typedef struct ui_oklch
{
    float L, C, h; /* lightness 0..1, chroma ~0..0.4, hue in degrees */
} ui_oklch;

ui_oklch ui_color_to_oklch(color_t c);
/* The colour with that lightness and hue and as much of the chroma as fits in
 * sRGB (chroma is reduced, lightness and hue are kept). */
color_t ui_oklch_to_color(ui_oklch c);
/* Same, but unrounded 0..255 channels, for callers that dither. */
void ui_oklch_to_rgbf(ui_oklch c, float rgb[3]);

/* A complete palette around the hues of two colours (the hue of `accent2` is
 * snapped to a harmony of the first's). */
ui_palette ui_palette_from_accent(color_t accent, color_t accent2);

/* Designs a palette for a w*h RGB24 picture (rows are w*3 bytes, no padding).
 * Pictures without any colour (greyscale, black, white) give a calm neutral
 * palette. Returns false only when there is no picture to look at. */
bool ui_palette_extract(const uint8_t *rgb, int w, int h, ui_palette *out);

/* Blend of two palettes, field by field. */
ui_palette ui_palette_mix(const ui_palette *a, const ui_palette *b, float t);

#endif /* __UI_PALETTE_H */
