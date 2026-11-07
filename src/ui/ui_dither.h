#ifndef __UI_DITHER_H
#define __UI_DITHER_H

/*
 * Turning a smooth colour field into terminal cells without banding.
 *
 * A dark gradient has only a handful of distinct 8-bit values, so rounding
 * every cell the same way draws visible contour lines. Dithering adds a fixed
 * pattern of noise before rounding so neighbouring cells land on either side
 * of the boundary and the eye averages them back into a smooth ramp. The
 * pattern depends on the cell's position only (never on time), so a page that
 * is not changing stays perfectly still.
 *
 * Truecolor only: with 256 colours the palette is so coarse that no pattern
 * helps, and the nearest palette colour is used.
 *
 * Pure: no toolkit, no application state.
 */

#include "color.h"

#include <stdint.h>

/* Static blue-ish noise in [0, 1) for a cell, different for each channel. */
float ui_dither_noise(int x, int y, int channel);

/* `rgb` is 0..255 per channel, unrounded. */
color_t ui_dither_rgb(const float rgb[3], int x, int y);

/* 256-colour terminals: the nearest palette colour, as a solid cell. */
void ui_dither_256(const float rgb[3], int x, int y, uint32_t *cp, color_t *fg,
                   color_t *bg);

#endif /* __UI_DITHER_H */
