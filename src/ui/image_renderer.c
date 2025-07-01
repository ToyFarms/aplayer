#include "image_renderer.h"
#include "term_draw.h"

static void image_render_halfblock(image_renderer_ctx *ctx, image_t *img)
{
    for (int y = 0; y < img->width; y += 2)
    {
        for (int x = 0; x < img->height; x++)
        {
            bool has_bottom = y + 1 < img->height;
            if (has_bottom)
            {
                int top_base = (y * img->width + x) * 3;
                int bot_base = ((y + 1) * img->width + x) * 3;
                term_draw_color(
                    &ctx->rendered,
                    COLOR(img->data[top_base + 0], img->data[top_base + 1],
                          img->data[top_base + 2]),
                    COLOR(img->data[bot_base + 0], img->data[bot_base + 1],
                          img->data[bot_base + 2]));
                str_cat(&ctx->rendered, "▄");
            }
            else
            {
                int top_base = (y * img->width + x) * 3;
                term_draw_color(&ctx->rendered, COLOR_NONE,
                                COLOR(img->data[top_base + 0],
                                      img->data[top_base + 1],
                                      img->data[top_base + 2]));
                str_cat(&ctx->rendered, "▀");
            }
        }
        term_draw_reset(&ctx->rendered);
        term_draw_move(&ctx->rendered, VEC(-img->width, 1));
    }
}

bool image_render(image_renderer_ctx *ctx, image_t *img,
                  enum image_render_method method)
{
    bool should_draw = ctx->redraw || img->width != ctx->prev_width ||
                       img->height != ctx->prev_height;

    if (should_draw)
    {
        ctx->rendered.len = 0;
        switch (method)
        {
        case IMAGE_RENDER_HALFBLOCK:
            image_render_halfblock(ctx, img);
            break;
        case IMAGE_RENDER_BLOCK:
            break;
        case IMAGE_RENDER_SIXEL:
            break;
        case IMAGE_RENDER_TGP:
            break;
        }
    }

    ctx->redraw = false;
    ctx->prev_width = img->width;
    ctx->prev_height = img->height;

    return should_draw;
}

image_renderer_ctx image_renderer_create()
{
    image_renderer_ctx ctx = {0};
    ctx.rendered = str_alloc(1024);

    return ctx;
}

void image_renderer_free(image_renderer_ctx *ctx)
{
    if (ctx == NULL)
        return;

    str_free(&ctx->rendered);
}
