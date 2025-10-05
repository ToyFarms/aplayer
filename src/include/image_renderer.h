#ifndef __IMAGE_RENDERER_H
#define __IMAGE_RENDERER_H

#include "_math.h"
#include "ds.h"
#include "image.h"

enum image_render_method
{
    IMAGE_RENDER_HALFBLOCK,
    IMAGE_RENDER_BRAILLE,
    IMAGE_RENDER_SIXEL,
    IMAGE_RENDER_TGP,
#define IMAGE_RENDER_LENGTH ((int)IMAGE_RENDER_TGP + 1)
};

void image_render(str_t *out, image_t *img, enum image_render_method method);
vec2 image_pixel_density(enum image_render_method method);

#endif /* __IMAGE_RENDERER_H */
