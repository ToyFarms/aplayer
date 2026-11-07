#include "ui_sprite.h"

#include "_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define OPAQUE_AT  128 /* alpha from which a sub-pixel counts as covered */
#define SOLID_BIAS 6.0f /* a glyph must beat a plain cell by this much */
#define CELL_PX    (UI_SUBCELL_W * UI_SUBCELL_H)

void ui_fb_resize(ui_fb *fb, int cells_w, int cells_h)
{
    int w = MATH_MAX(cells_w, 0) * UI_SUBCELL_W;
    int h = MATH_MAX(cells_h, 0) * UI_SUBCELL_H;
    if (fb->w != w || fb->h != h)
    {
        free(fb->px);
        fb->px = w > 0 && h > 0 ? calloc((size_t)w * (size_t)h, 4) : NULL;
        if (fb->px == NULL && w > 0 && h > 0)
            abort();
        fb->w = w;
        fb->h = h;
    }
    ui_fb_clear(fb);
}

void ui_fb_clear(ui_fb *fb)
{
    if (fb->px)
        memset(fb->px, 0, (size_t)fb->w * (size_t)fb->h * 4);
}

void ui_fb_free(ui_fb *fb)
{
    free(fb->px);
    *fb = (ui_fb){0};
}

/* ------------------------------------------------------------------------ */
/* sprites                                                                  */
/* ------------------------------------------------------------------------ */

/* Position inside the sprite, in its own (unrotated) frame, and how much of
 * the pixel the rounded rectangle covers (0..1, one pixel of anti-aliasing).
 * `lx`/`ly` are relative to the sprite's centre. */
static float coverage(const ui_sprite *s, float x, float y, float *lx,
                      float *ly)
{
    float dx = x - s->cx, dy = y - s->cy;
    float c = cosf(s->angle), sn = sinf(s->angle);
    *lx = dx * c + dy * sn;
    *ly = -dx * sn + dy * c;

    float hw = s->width * 0.5f, hh = s->height * 0.5f;
    float r = MATH_CLAMP(s->radius, 0.0f, MATH_MIN(hw, hh));
    float qx = fabsf(*lx) - (hw - r), qy = fabsf(*ly) - (hh - r);
    float outside = hypotf(MATH_MAX(qx, 0.0f), MATH_MAX(qy, 0.0f));
    float dist = outside + MATH_MIN(MATH_MAX(qx, qy), 0.0f) - r;
    return MATH_CLAMP(0.5f - dist, 0.0f, 1.0f);
}

bool ui_sprite_contains(const ui_sprite *s, float x, float y)
{
    float lx, ly;
    return coverage(s, x, y, &lx, &ly) >= 0.5f;
}

static void sample(const ui_sprite *s, float u, float v, float out[3])
{
    float fx = MATH_CLAMP(u * (float)s->w - 0.5f, 0.0f, (float)(s->w - 1));
    float fy = MATH_CLAMP(v * (float)s->h - 0.5f, 0.0f, (float)(s->h - 1));
    int x0 = (int)fx, y0 = (int)fy;
    int x1 = MATH_MIN(x0 + 1, s->w - 1), y1 = MATH_MIN(y0 + 1, s->h - 1);
    float tx = fx - (float)x0, ty = fy - (float)y0;

    const uint8_t *p00 = s->rgb + ((size_t)y0 * (size_t)s->w + (size_t)x0) * 3;
    const uint8_t *p10 = s->rgb + ((size_t)y0 * (size_t)s->w + (size_t)x1) * 3;
    const uint8_t *p01 = s->rgb + ((size_t)y1 * (size_t)s->w + (size_t)x0) * 3;
    const uint8_t *p11 = s->rgb + ((size_t)y1 * (size_t)s->w + (size_t)x1) * 3;
    for (int c = 0; c < 3; c++)
    {
        float top = (float)p00[c] * (1.0f - tx) + (float)p10[c] * tx;
        float bot = (float)p01[c] * (1.0f - tx) + (float)p11[c] * tx;
        out[c] = top * (1.0f - ty) + bot * ty;
    }
}

void ui_fb_draw(ui_fb *fb, const ui_sprite *s)
{
    if (fb->px == NULL || s->rgb == NULL || s->w <= 0 || s->h <= 0 ||
        s->width <= 0.0f || s->height <= 0.0f || s->alpha <= 0.0f)
        return;

    /* bounding box of the rotated rectangle, plus the anti-aliasing fringe */
    float c = fabsf(cosf(s->angle)), sn = fabsf(sinf(s->angle));
    float ex = (s->width * c + s->height * sn) * 0.5f + 1.0f;
    float ey = (s->width * sn + s->height * c) * 0.5f + 1.0f;
    int x0 = MATH_MAX((int)floorf(s->cx - ex), 0);
    int x1 = MATH_MIN((int)ceilf(s->cx + ex), fb->w);
    int y0 = MATH_MAX((int)floorf(s->cy - ey), 0);
    int y1 = MATH_MIN((int)ceilf(s->cy + ey), fb->h);

    float shade = MATH_CLAMP(s->shade, 0.0f, 1.0f);
    for (int y = y0; y < y1; y++)
    {
        for (int x = x0; x < x1; x++)
        {
            float lx, ly;
            float a = coverage(s, (float)x + 0.5f, (float)y + 0.5f, &lx, &ly) *
                      MATH_CLAMP(s->alpha, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;

            float rgb[3];
            sample(s, lx / s->width + 0.5f, ly / s->height + 0.5f, rgb);
            rgb[0] = rgb[0] * (1.0f - shade) + (float)s->fog.r * shade;
            rgb[1] = rgb[1] * (1.0f - shade) + (float)s->fog.g * shade;
            rgb[2] = rgb[2] * (1.0f - shade) + (float)s->fog.b * shade;

            uint8_t *d = fb->px + ((size_t)y * (size_t)fb->w + (size_t)x) * 4;
            float da = (float)d[3] / 255.0f;
            float oa = a + da * (1.0f - a);
            for (int k = 0; k < 3; k++)
                d[k] = (uint8_t)MATH_CLAMP(
                    (rgb[k] * a + (float)d[k] * da * (1.0f - a)) / oa + 0.5f,
                    0.0f, 255.0f);
            d[3] = (uint8_t)MATH_CLAMP(oa * 255.0f + 0.5f, 0.0f, 255.0f);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* framebuffer -> cells                                                     */
/* ------------------------------------------------------------------------ */

typedef struct group
{
    float sum[3];
    int n;
} group;

static const float WEIGHT[3] = {0.299f, 0.587f, 0.114f};

/* Masks worth trying for a two-colour cell. A mask and its complement are the
 * same split with the colours swapped, so only one of each pair is listed. */
static const uint8_t SPLITS[] = {0x0F, 0x55, 0x05, 0x0A, 0x50,
                                 0xA0, 0xA5, 0x5A, 0x03, 0xC0};

static color_t mean(const group *g)
{
    if (g->n == 0)
        return COLOR_NONE;
    return COLOR((uint8_t)(g->sum[0] / (float)g->n + 0.5f),
                 (uint8_t)(g->sum[1] / (float)g->n + 0.5f),
                 (uint8_t)(g->sum[2] / (float)g->n + 0.5f));
}

/* Squared perceptual error of painting `px` with the means of the two groups
 * `mask` selects. Fills `fg` (mask set) and `bg` (mask clear). */
static float split_error(const float px[CELL_PX][3], uint8_t mask,
                         color_t *fg, color_t *bg)
{
    group on = {0}, off = {0};
    for (int i = 0; i < CELL_PX; i++)
    {
        group *g = (mask >> i) & 1 ? &on : &off;
        for (int c = 0; c < 3; c++)
            g->sum[c] += px[i][c];
        g->n++;
    }
    *fg = mean(&on);
    *bg = mean(&off);

    float err = 0.0f;
    for (int i = 0; i < CELL_PX; i++)
    {
        const group *g = (mask >> i) & 1 ? &on : &off;
        for (int c = 0; c < 3; c++)
        {
            float d = px[i][c] - g->sum[c] / (float)g->n;
            err += WEIGHT[c] * d * d;
        }
    }
    return err;
}

static float luma(const float px[3])
{
    return WEIGHT[0] * px[0] + WEIGHT[1] * px[1] + WEIGHT[2] * px[2];
}

static void fit_full(const float px[CELL_PX][3], enum ui_fb_glyphs mode,
                     ui_glyph_fn glyph, ui_cell *cell)
{
    color_t fg, bg;
    float best = split_error(px, 0xFF, &fg, &bg); /* one flat colour */
    uint8_t best_mask = 0xFF;
    color_t best_fg = fg, best_bg = bg;

    if (mode == UI_FB_BRAILLE)
    {
        float mid = 0.0f;
        for (int i = 0; i < CELL_PX; i++)
            mid += luma(px[i]);
        mid /= (float)CELL_PX;
        uint8_t mask = 0;
        for (int i = 0; i < CELL_PX; i++)
            if (luma(px[i]) > mid)
                mask |= (uint8_t)(1u << i);
        if (mask != 0 && mask != 0xFF)
        {
            best_mask = mask;
            split_error(px, mask, &best_fg, &best_bg);
        }
    }
    else
    {
        for (size_t k = 0; k < sizeof(SPLITS); k++)
        {
            float err = split_error(px, SPLITS[k], &fg, &bg) + SOLID_BIAS;
            if (err < best)
            {
                best = err;
                best_mask = SPLITS[k];
                best_fg = fg;
                best_bg = bg;
            }
        }
    }

    if (best_mask == 0xFF)
    {
        split_error(px, 0xFF, &fg, &bg);
        *cell = (ui_cell){.cp = ' ', .fg = COLOR_NONE, .bg = fg};
        return;
    }
    cell->cp = glyph(best_mask);
    cell->fg = best_fg;
    cell->bg = best_bg;
}

void ui_fb_to_cells(const ui_fb *fb, ui_cell *grid, enum ui_fb_glyphs mode,
                    ui_glyph_fn glyph)
{
    int cw = fb->w / UI_SUBCELL_W, ch = fb->h / UI_SUBCELL_H;
    for (int cy = 0; cy < ch; cy++)
    {
        for (int cx = 0; cx < cw; cx++)
        {
            float px[CELL_PX][3];
            float seen[3] = {0, 0, 0};
            uint8_t covered = 0;
            int n = 0;

            for (int row = 0; row < UI_SUBCELL_H; row++)
            {
                for (int col = 0; col < UI_SUBCELL_W; col++)
                {
                    int i = row * UI_SUBCELL_W + col;
                    const uint8_t *p =
                        fb->px + ((size_t)(cy * UI_SUBCELL_H + row) *
                                      (size_t)fb->w +
                                  (size_t)(cx * UI_SUBCELL_W + col)) *
                                     4;
                    for (int c = 0; c < 3; c++)
                        px[i][c] = (float)p[c];
                    if (p[3] >= OPAQUE_AT)
                    {
                        covered |= (uint8_t)(1u << i);
                        for (int c = 0; c < 3; c++)
                            seen[c] += (float)p[c];
                        n++;
                    }
                }
            }

            ui_cell *cell = &grid[cy * cw + cx];
            *cell = (ui_cell){0};
            if (covered == 0)
                continue;

            if (covered == 0xFF)
            {
                fit_full(px, mode, glyph, cell);
                continue;
            }

            /* partly covered: the covered sub-pixels in their own colour,
             * the rest left to whatever is underneath */
            cell->cp = glyph(covered);
            cell->fg = COLOR((uint8_t)(seen[0] / (float)n + 0.5f),
                             (uint8_t)(seen[1] / (float)n + 0.5f),
                             (uint8_t)(seen[2] / (float)n + 0.5f));
            cell->bg = COLOR_NONE;
        }
    }
}
