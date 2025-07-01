#ifndef __IMAGE_RENDERER_H
#define __IMAGE_RENDERER_H

#include "ds.h"
#include "image.h"

typedef struct image_renderer_ctx
{
    str_t rendered;
    int prev_width;
    int prev_height;
    bool redraw;
} image_renderer_ctx;

enum image_render_method
{
    IMAGE_RENDER_HALFBLOCK,
    IMAGE_RENDER_BLOCK,
    IMAGE_RENDER_SIXEL,
    IMAGE_RENDER_TGP,
};

image_renderer_ctx image_renderer_create();
void image_renderer_free(image_renderer_ctx *ctx);
bool image_render(image_renderer_ctx *ctx, image_t *img,
                  enum image_render_method method);

#endif /* __IMAGE_RENDERER_H */
