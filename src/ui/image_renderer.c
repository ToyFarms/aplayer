#include "image_renderer.h"
#include "_math.h"
#include "app.h"
#include "ds.h"
#include "imgconv.h"
#include "term.h"
#include "term_draw.h"
#include <sixel.h>

#define PXR                  0
#define PXG                  1
#define PXB                  2
#define GET_PX(img, x, y, z) ((img)->data[((y) * (img)->width + (x)) * 3 + (z)])

#define SUBCELL_W 2
#define SUBCELL_H 4
#define SUBCELL_N (SUBCELL_W * SUBCELL_H)

#define SPIDX(c, r) ((r) * SUBCELL_W + (c))

static const float w_rgb[3] = {0.299f, 0.587f, 0.114f};

static wchar_t mask_to_braille(uint8_t mask)
{
    uint8_t b = 0;
    if (mask & (1u << SPIDX(0, 0)))
        b |= 1u << 0;
    if (mask & (1u << SPIDX(0, 1)))
        b |= 1u << 1;
    if (mask & (1u << SPIDX(0, 2)))
        b |= 1u << 2;
    if (mask & (1u << SPIDX(1, 0)))
        b |= 1u << 3;
    if (mask & (1u << SPIDX(1, 1)))
        b |= 1u << 4;
    if (mask & (1u << SPIDX(1, 2)))
        b |= 1u << 5;
    if (mask & (1u << SPIDX(0, 3)))
        b |= 1u << 6;
    if (mask & (1u << SPIDX(1, 3)))
        b |= 1u << 7;
    return (wchar_t)(0x2800u + b);
}

typedef struct
{
    wchar_t ch;
    uint8_t mask;
    int bias;
} glyph_t;

static const glyph_t glyphs[] = {
    {L' ', 0x00, 0},
    {L'█', 0xFF, 0},
    {L'▀', 0x0F, 0},
    {L'▄', 0xF0, 0},
    {L'▌', 0x55, 0},
    {L'▐', 0xAA, 0},
    {L'▔', 0x03, 0},
    {L'▂', 0xC0, 0},
    {L'▆', 0xFC, 0},

    {L'▘', 0x05, 0},
    {L'▝', 0x0A, 0},
    {L'▖', 0x50, 0},
    {L'▗', 0xA0, 0},

    {L'▚', 0xA5, 0},
    {L'▞', 0x5A, 0},

    {L'▛', 0x5F, 0},
    {L'▜', 0xAF, 0},
    {L'▙', 0xF5, 0},
    {L'▟', 0xFA, 0},

    {L'░', 0xFF, 3},
    {L'▒', 0xFF, 4},
    {L'▓', 0xFF, 5},
};

static wchar_t pick_glyph_for_mask(uint8_t mask)
{
    wchar_t best = L' ';
    int best_score = 1 << 30;

    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++)
    {
        uint8_t g = glyphs[i].mask;

        uint8_t diff = (uint8_t)(mask ^ g);
        int d = __builtin_popcount((unsigned)diff);

        int score = d * 10 + glyphs[i].bias;

        if (score < best_score)
        {
            best_score = score;
            best = glyphs[i].ch;
        }
    }

    if (best == L' ')
        return mask_to_braille(mask);

    return best;
}

static const uint8_t bayer8[8][8] = {
    {0,  32, 8,  40, 2,  34, 10, 42},
    {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44, 4,  36, 14, 46, 6,  38},
    {60, 28, 52, 20, 62, 30, 54, 22},
    {3,  35, 11, 43, 1,  33, 9,  41},
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7,  39, 13, 45, 5,  37},
    {63, 31, 55, 23, 61, 29, 53, 21},
};

#define DITHER_STRENGTH 12.0f
#define CLAMPF255(v)    ((v) < 0.0f ? 0.0f : (v) > 255.0f ? 255.0f : (v))

static void image_render_glyph(str_t *out, image_t *img)
{
    int cells_x = img->width / SUBCELL_W;
    int cells_y = img->height / SUBCELL_H;

    if (cells_x <= 0 || cells_y <= 0)
        return;

    for (int cy = 0; cy < cells_y; cy++)
    {
        for (int cx = 0; cx < cells_x; cx++)
        {
            float px[SUBCELL_N][3];

            for (int row = 0; row < SUBCELL_H; row++)
            {
                int iy = cy * SUBCELL_H + row;
                for (int col = 0; col < SUBCELL_W; col++)
                {
                    int ix = cx * SUBCELL_W + col;
                    int i = SPIDX(col, row);

                    float doff = (bayer8[iy & 7][ix & 7] / 63.0f - 0.5f) *
                                 DITHER_STRENGTH;

                    px[i][0] =
                        CLAMPF255((float)GET_PX(img, ix, iy, PXR) + doff);
                    px[i][1] =
                        CLAMPF255((float)GET_PX(img, ix, iy, PXG) + doff);
                    px[i][2] =
                        CLAMPF255((float)GET_PX(img, ix, iy, PXB) + doff);
                }
            }

            float best_err = 1e30f;
            uint8_t best_mask = 0;
            float best_fg[3] = {0, 0, 0};
            float best_bg[3] = {0, 0, 0};

            for (int mask = 0; mask < 256; mask++)
            {
                float sum_fg[3] = {0, 0, 0};
                float sum_bg[3] = {0, 0, 0};
                int cnt_fg = 0;
                int cnt_bg = 0;

                for (int i = 0; i < SUBCELL_N; i++)
                {
                    if (mask & (1 << i))
                    {
                        sum_fg[0] += px[i][0];
                        sum_fg[1] += px[i][1];
                        sum_fg[2] += px[i][2];
                        cnt_fg++;
                    }
                    else
                    {
                        sum_bg[0] += px[i][0];
                        sum_bg[1] += px[i][1];
                        sum_bg[2] += px[i][2];
                        cnt_bg++;
                    }
                }

                float avg_fg[3] = {0, 0, 0};
                float avg_bg[3] = {0, 0, 0};

                if (cnt_fg)
                {
                    avg_fg[0] = sum_fg[0] / cnt_fg;
                    avg_fg[1] = sum_fg[1] / cnt_fg;
                    avg_fg[2] = sum_fg[2] / cnt_fg;
                }
                if (cnt_bg)
                {
                    avg_bg[0] = sum_bg[0] / cnt_bg;
                    avg_bg[1] = sum_bg[1] / cnt_bg;
                    avg_bg[2] = sum_bg[2] / cnt_bg;
                }

                float err = 0.0f;
                for (int i = 0; i < SUBCELL_N; i++)
                {
                    const float *avg = (mask & (1 << i)) ? avg_fg : avg_bg;
                    for (int c = 0; c < 3; c++)
                    {
                        float d = px[i][c] - avg[c];
                        err += w_rgb[c] * d * d;
                    }
                }

                if (err < best_err)
                {
                    best_err = err;
                    best_mask = (uint8_t)mask;
                    best_fg[0] = avg_fg[0];
                    best_fg[1] = avg_fg[1];
                    best_fg[2] = avg_fg[2];
                    best_bg[0] = avg_bg[0];
                    best_bg[1] = avg_bg[1];
                    best_bg[2] = avg_bg[2];
                }
            }

            wchar_t glyph = pick_glyph_for_mask(best_mask);

            if (best_mask == 0x00)
            {
                term_draw_color(out,
                                COLOR((uint8_t)best_bg[0], (uint8_t)best_bg[1],
                                      (uint8_t)best_bg[2]),
                                COLOR_NONE);
            }
            else if (best_mask == 0xFF)
            {
                term_draw_color(out, COLOR_NONE,
                                COLOR((uint8_t)best_fg[0], (uint8_t)best_fg[1],
                                      (uint8_t)best_fg[2]));
            }
            else
            {
                term_draw_color(out,
                                COLOR((uint8_t)best_bg[0], (uint8_t)best_bg[1],
                                      (uint8_t)best_bg[2]),
                                COLOR((uint8_t)best_fg[0], (uint8_t)best_fg[1],
                                      (uint8_t)best_fg[2]));
            }
            str_catwch(out, glyph);
        }
        term_draw_reset(out);
        term_draw_move(out, VEC(-cells_x, 1));
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

static void image_render_sixel(str_t *str_out, image_t *img,
                               const term_capability *cap)
{
    sixel_output_t *output = NULL;
    sixel_dither_t *dither = NULL;
    int result;
    str_t tmp = str_create();

    result = sixel_output_new(&output, write_fn, &tmp, NULL);
    if (SIXEL_FAILED(result))
        goto cleanup;

    result = sixel_dither_new(&dither, 256, NULL);
    if (SIXEL_FAILED(result))
        goto cleanup;

    result = sixel_dither_initialize(dither, img->data, img->width, img->height,
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

    if (cap->is_tmux)
    {
        str_cat(str_out, "\x1bPtmux;");

        STR_SPLIT(tmp, chunk, "\x1b")
        {
            str_cat(str_out, "\x1b\x1b");
            if (chunk.len == 0)
                continue;
            str_cat_str(str_out, &strv(chunk));
        }

        str_cat(str_out, "\x1b\\");
    }
    else
    {
        str_cat_str(str_out, &tmp);
    }

cleanup:
    if (dither)
        sixel_dither_unref(dither);
    if (output)
        sixel_output_unref(output);
    str_free(&tmp);
}

void image_render(str_t *out, image_t *img, enum image_render_method method,
                  const term_capability *cap)
{
    switch (method)
    {
    case IMAGE_RENDER_GLYPH:
        image_render_glyph(out, img);
        break;
    case IMAGE_RENDER_BRAILLE:
        image_render_braille(out, img);
        break;
    case IMAGE_RENDER_SIXEL:
        image_render_sixel(out, img, cap);
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
    case IMAGE_RENDER_GLYPH:
        return VEC(2, 4);
    case IMAGE_RENDER_BRAILLE:
        return VEC(2, 4);
    case IMAGE_RENDER_SIXEL:
        return VEC(10, 20);
    case IMAGE_RENDER_TGP:
        return VEC(1, 1);
    default:
        return VEC(1, 1);
    }
}
