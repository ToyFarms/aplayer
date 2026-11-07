#ifndef __UI_ANSI_H
#define __UI_ANSI_H

/*
 * Bridge between the image renderers (which emit escape-sequence streams) and
 * the toolkit canvas (which paints cells): decoding the stream once into a
 * cell grid lets pictures take part in damage tracking and diffing like any
 * other widget instead of being written straight to the terminal.
 */

#include "color.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ui_cell
{
    uint32_t cp; /* codepoint, 0 = untouched cell */
    color_t fg, bg;
    bool reverse;
} ui_cell;

/* Decodes the stream `s` (cursor moves A/B/C/D/H, SGR colours incl. 256-colour
 * and truecolour, newlines, UTF-8 glyphs) into a w*h grid. Anything else is
 * skipped; writes outside the grid are dropped. */
void ui_ansi_decode(const char *s, size_t len, ui_cell *grid, int w, int h);

#endif /* __UI_ANSI_H */
