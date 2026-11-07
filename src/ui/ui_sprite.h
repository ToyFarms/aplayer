#ifndef __UI_SPRITE_H
#define __UI_SPRITE_H

/*
 * Pictures that move: a small software compositor for the cover art stack.
 *
 * image_render() turns one finished picture into cells with an exhaustive
 * search per cell: right for a still, far too slow to run every frame, and it
 * knows nothing about transparency. Cards that slide, tilt and overlap need
 * both, so they are composited here into a sub-pixel framebuffer (the same
 * 2x4 sub-pixels per cell as the glyph renderer) and fitted to cells with a
 * handful of candidate glyphs per cell. Cells the sprites do not touch stay
 * empty, so whatever is painted underneath shows through.
 *
 * Pure: no toolkit, no application state.
 */

#include "color.h"
#include "ui_ansi.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_SUBCELL_W 2
#define UI_SUBCELL_H 4

typedef struct ui_fb
{
    int w, h;    /* sub-pixels */
    uint8_t *px; /* w * h * 4, RGBA, straight alpha */
} ui_fb;

/* (Re)allocates for a grid of cells_w x cells_h cells, cleared. */
void ui_fb_resize(ui_fb *fb, int cells_w, int cells_h);
void ui_fb_clear(ui_fb *fb);
void ui_fb_free(ui_fb *fb);

typedef struct ui_sprite
{
    const uint8_t *rgb; /* source picture, RGB24, rows are w * 3 bytes */
    int w, h;

    float cx, cy;          /* centre, in framebuffer sub-pixels */
    float width, height;   /* drawn size, in sub-pixels, before rotating */
    float angle;           /* radians, clockwise on screen */
    float radius;          /* corner radius, in sub-pixels */
    float shade;           /* 0: untouched .. 1: all `fog` */
    color_t fog;
    float alpha;           /* opacity of the whole sprite, 0..1 */
} ui_sprite;

/* Draws over what is already in the framebuffer. */
void ui_fb_draw(ui_fb *fb, const ui_sprite *s);

/* Does the sprite cover the sub-pixel position (x, y)? */
bool ui_sprite_contains(const ui_sprite *s, float x, float y);

enum ui_fb_glyphs
{
    UI_FB_BLOCKS,  /* block elements, two colours per cell */
    UI_FB_BRAILLE, /* braille dots, two colours per cell */
};

/* The character that lights the sub-pixels of `mask` (bit = row * 2 + column,
 * row 0 on top). The renderers own these tables: image_mask_to_glyph() and
 * image_mask_to_braille() are what the application passes in. */
typedef uint32_t (*ui_glyph_fn)(uint8_t mask);

/* Fits the framebuffer to a (w / 2) x (h / 4) grid. Cells without any covered
 * sub-pixel get cp == 0; cells covered in part get a glyph in the picture's
 * colour over a transparent background (bg alpha 0). */
void ui_fb_to_cells(const ui_fb *fb, ui_cell *grid, enum ui_fb_glyphs mode,
                    ui_glyph_fn glyph);

#endif /* __UI_SPRITE_H */
