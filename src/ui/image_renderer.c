#include "image_renderer.h"
#include "_math.h"
#include "app.h"
#include "imgconv.h"
#include "term.h"
#include "term_draw.h"
#include <sixel.h>

#define PXR                  0
#define PXG                  1
#define PXB                  2
#define GET_PX(img, x, y, z) ((img)->data[((y) * (img)->width + (x)) * 3 + (z)])

static void image_render_halfblock(str_t *out, image_t *img)
{
    for (int y = 0; y < img->height; y += 2)
    {
        for (int x = 0; x < img->width; x++)
        {
            bool has_bottom = y + 1 < img->height;
            if (has_bottom)
            {
                term_draw_color(out,
                                COLOR(GET_PX(img, x, y, PXR),
                                      GET_PX(img, x, y, PXG),
                                      GET_PX(img, x, y, PXB)),
                                COLOR(GET_PX(img, x, y + 1, PXR),
                                      GET_PX(img, x, y + 1, PXG),
                                      GET_PX(img, x, y + 1, PXB)));
                str_cat(out, "▄");
            }
            else
            {
                term_draw_color(out, COLOR_NONE,
                                COLOR(GET_PX(img, x, y, PXR),
                                      GET_PX(img, x, y, PXG),
                                      GET_PX(img, x, y, PXB)));
                str_cat(out, "▀");
            }
        }
        term_draw_reset(out);
        term_draw_move(out, VEC(-img->width, 1));
    }
}

static int coord_to_offset(uint8_t x, uint8_t y)
{
    if (x < 0 || x >= 2 || y < 0 || y >= 4)
        return 0;
#define XY(x, y) ((x) << 8 | (y))

    switch (XY(x, y))
    {
    case XY(0, 0):
        return 0;
    case XY(0, 1):
        return 1;
    case XY(0, 2):
        return 2;
    case XY(1, 0):
        return 3;
    case XY(1, 1):
        return 4;
    case XY(1, 2):
        return 5;
    case XY(0, 3):
        return 6;
    case XY(1, 3):
        return 7;
    }

    return 0;
}

static void image_render_braille(str_t *out, image_t *img)
{
    image_t *orig = img;

    const char *filters = "[in]split=3[g1][g2][g3];"
                          "[g3]format=gray,edgedetect,sobel,negate[edge];"
                          "[g1]palettegen=reserve_transparent=0:"
                          "stats_mode=single[pal];"
                          "[g2][pal]paletteuse=dither=bayer:bayer_scale=1[di];"
                          "[di][edge]blend=all_mode=softlight,"
                          "format=rgb24[out]";

    imgconv_frame frame = imgconv_filter_chain(
        img->data, img->width, img->height, AV_PIX_FMT_RGB24, filters);
    image_t dither = image_from_frame(&frame);
    img = &dither;

    const int base = 0x2800;

    int width = (img->width + 1) / 2;
    int height = (img->height + 3) / 4;
    for (int block_y = 0; block_y < height; block_y++)
    {
        int px_y = block_y * 4;
        for (int block_x = 0; block_x < width; block_x++)
        {
            int px_x = block_x * 2;
            uint8_t code = 0;

            float sum_r = 0, sum_g = 0, sum_b = 0;
            int count = 0;

            for (int suby = 0; suby < 4; suby++)
            {
                for (int subx = 0; subx < 2; subx++)
                {
                    int x = px_x + subx;
                    int y = px_y + suby;
                    if (x >= 0 && x < img->width && y >= 0 && y < img->height)
                    {
                        float r = GET_PX(img, x, y, PXR) / 255.0f;
                        float g = GET_PX(img, x, y, PXG) / 255.0f;
                        float b = GET_PX(img, x, y, PXB) / 255.0f;
                        float luminance =
                            0.2126f * r + 0.7152f * g + 0.0722f * b;

                        if (luminance > 0.3f)
                        {
                            code |= (1 << coord_to_offset(subx, suby));

                            float r = GET_PX(orig, x, y, PXR);
                            float g = GET_PX(orig, x, y, PXG);
                            float b = GET_PX(orig, x, y, PXB);
                            sum_r += r;
                            sum_g += g;
                            sum_b += b;
                            count++;
                        }
                    }
                }
            }

            if (code > 0)
            {
                if (count > 0)
                {
                    uint8_t avg_r = sum_r / count;
                    uint8_t avg_g = sum_g / count;
                    uint8_t avg_b = sum_b / count;
                    term_draw_color(out, COLOR_NONE,
                                    COLOR(avg_r, avg_g, avg_b));
                }
                str_catwch(out, (wchar_t)(base + code));
                term_draw_reset(out);
            }
            else
                str_catch(out, ' ');
        }
        term_draw_move(out, VEC(-(img->width + 1) / 2, 1));
    }

    free(frame.buffer);
}

static int write_fn(char *buf, int len, void *userdata)
{
    str_t *s = userdata;
    str_catlen(s, buf, len);

    return 0;
}

// NOTE: i would like to implement this function myself, but achieving usable
// result with less than desirable lag is really time consuming. more reading:
// - https://qiita.com/arakiken/items/4a216af6547d2574d283
static void image_render_sixel(str_t *str_out, image_t *img)
{
    sixel_output_t *output = NULL;
    sixel_dither_t *dither = NULL;
    int result;

    result = sixel_output_new(&output, write_fn, str_out, NULL);
    if (SIXEL_FAILED(result))
        goto cleanup;

    result = sixel_dither_new(&dither, 256, NULL);
    if (SIXEL_FAILED(result))
        goto cleanup;

    result =
        sixel_dither_initialize(dither, img->data, img->height, img->height,
                                SIXEL_PIXELFORMAT_RGB888, 0, 0, 3);
    if (SIXEL_FAILED(result))
        goto cleanup;
    sixel_dither_set_diffusion_type(dither, DIFFUSE_FS);

    sixel_dither_set_body_only(dither, 0);
    sixel_dither_set_optimize_palette(dither, 1);

    result = sixel_encode((unsigned char *)img->data, img->width, img->height,
                          3, dither, output);

    if (SIXEL_FAILED(result))
        goto cleanup;

cleanup:
    if (dither)
        sixel_dither_unref(dither);
    if (output)
        sixel_output_unref(output);
}

void image_render(str_t *out, image_t *img, enum image_render_method method)
{
    switch (method)
    {
    case IMAGE_RENDER_HALFBLOCK:
        image_render_halfblock(out, img);
        break;
    case IMAGE_RENDER_BRAILLE:
        image_render_braille(out, img);
        break;
    case IMAGE_RENDER_SIXEL:
        image_render_sixel(out, img);
        break;
    case IMAGE_RENDER_TGP:
        break;
    }
}

vec2 image_pixel_density(enum image_render_method method)
{
    app_instance *app = app_get();

    switch (method)
    {
    case IMAGE_RENDER_HALFBLOCK:
        return VEC(1, 2);
    case IMAGE_RENDER_BRAILLE:
        return VEC(2, 4);
    case IMAGE_RENDER_SIXEL:
        return VEC(6, app->term.capability.cell_height);
    case IMAGE_RENDER_TGP:
        return VEC(1, 1);
    default:
        return VEC(1, 1);
    }
}
