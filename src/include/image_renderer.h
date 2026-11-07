#ifndef __IMAGE_RENDERER_H
#define __IMAGE_RENDERER_H

#include "_math.h"
#include "ds.h"
#include "image.h"
#include "term.h"

enum image_render_method
{
    IMAGE_RENDER_GLYPH,
    IMAGE_RENDER_BRAILLE,
    IMAGE_RENDER_SIXEL,
    IMAGE_RENDER_TGP,
#define IMAGE_RENDER_LENGTH ((int)IMAGE_RENDER_TGP + 1)
};

/* How image_fit_size() maps an image onto a requested box of terminal cells. */
enum image_fit_mode
{
    IMAGE_FIT_AUTO,    /* derive the missing axis from the aspect ratio */
    IMAGE_FIT_EXACT,   /* use both requested axes as-is */
    IMAGE_FIT_WIDTH,   /* use requested width, derive height from aspect */
    IMAGE_FIT_HEIGHT,  /* use requested height, derive width from aspect */
    IMAGE_FIT_CONTAIN, /* fit inside the box, preserve aspect */
    IMAGE_FIT_COVER,   /* fill the box, preserve aspect */
};

void image_render(str_t *out, image_t *img, vec2 pos,
                  enum image_render_method method, const term_capability *cap);
vec2 image_pixel_density(enum image_render_method method);

/* The glyph renderers split every cell into 2x4 sub-pixels; a mask has one bit
 * per sub-pixel, bit = row * 2 + column with row 0 on top. These give the
 * character that lights exactly (or, failing that, nearest to) those
 * sub-pixels, so callers drawing their own cells use the same shapes. */
wchar_t image_mask_to_glyph(uint8_t mask);   /* blocks, braille as fallback */
wchar_t image_mask_to_braille(uint8_t mask); /* always a braille pattern */

/* Size in terminal cells at which `img` should be rendered with `method` to
 * satisfy `requested` (cells) under `mode`. */
vec2 image_fit_size(const image_t *img, enum image_render_method method,
                    vec2 requested, enum image_fit_mode mode);

#endif /* __IMAGE_RENDERER_H */
