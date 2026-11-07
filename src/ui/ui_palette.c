#include "ui_palette.h"

#include "_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_GRID 48
#define HUE_BINS    36
#define HUE_BIN_DEG (360.0f / HUE_BINS)
#define HUE_APART   4 /* bins between the two hues */
#define MIN_CHROMA  0.045f
#define MIN_L       0.18f
#define MAX_L       0.96f
#define MIN_COLOURFUL 0.04f /* share of pixels with a hue, else "no colour" */
#define SECOND_MIN  0.25f   /* second hue needs this share of the first */
#define NEUTRAL_HUE 252.0f  /* calm blue-slate for pictures without a hue */

/* ------------------------------------------------------------------------ */
/* OKLab                                                                    */
/* ------------------------------------------------------------------------ */

static float to_lin(float c)
{
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

static float from_lin(float c)
{
    return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

static float wrap_hue(float h)
{
    h = fmodf(h, 360.0f);
    return h < 0.0f ? h + 360.0f : h;
}

/* signed shortest way from a to b, in (-180, 180] */
static float hue_delta(float a, float b)
{
    float d = wrap_hue(b - a);
    return d > 180.0f ? d - 360.0f : d;
}

ui_oklch ui_color_to_oklch(color_t c)
{
    float r = to_lin((float)c.r / 255.0f), g = to_lin((float)c.g / 255.0f),
          b = to_lin((float)c.b / 255.0f);
    float l = cbrtf(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    float m = cbrtf(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    float s = cbrtf(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
    float L = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    float A = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    float B = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
    float C = sqrtf(A * A + B * B);
    return (ui_oklch){L, C, C < 1e-4f ? 0.0f : wrap_hue(atan2f(B, A) * 57.29578f)};
}

/* linear sRGB of an OKLab colour; true when inside the gamut */
static bool lab_to_lin(float L, float A, float B, float out[3])
{
    float l = L + 0.3963377774f * A + 0.2158037573f * B;
    float m = L - 0.1055613458f * A - 0.0638541728f * B;
    float s = L - 0.0894841775f * A - 1.2914855480f * B;
    l = l * l * l;
    m = m * m * m;
    s = s * s * s;
    out[0] = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    out[1] = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    out[2] = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    const float e = 0.0005f;
    return out[0] >= -e && out[0] <= 1 + e && out[1] >= -e && out[1] <= 1 + e &&
           out[2] >= -e && out[2] <= 1 + e;
}

void ui_oklch_to_rgbf(ui_oklch c, float rgb[3])
{
    float L = MATH_CLAMP(c.L, 0.0f, 1.0f);
    float cs = cosf(c.h * 0.01745329f), sn = sinf(c.h * 0.01745329f);
    float lin[3];

    /* biggest chroma <= c.C that is inside the gamut (hue, lightness kept) */
    float lo = 0.0f, hi = MATH_MAX(c.C, 0.0f);
    if (!lab_to_lin(L, hi * cs, hi * sn, lin))
    {
        for (int i = 0; i < 16; i++)
        {
            float mid = (lo + hi) * 0.5f;
            if (lab_to_lin(L, mid * cs, mid * sn, lin))
                lo = mid;
            else
                hi = mid;
        }
        hi = lo;
    }
    lab_to_lin(L, hi * cs, hi * sn, lin);
    for (int i = 0; i < 3; i++)
        rgb[i] = 255.0f * MATH_CLAMP(from_lin(MATH_CLAMP(lin[i], 0.0f, 1.0f)),
                                     0.0f, 1.0f);
}

color_t ui_oklch_to_color(ui_oklch c)
{
    float f[3];
    ui_oklch_to_rgbf(c, f);
    return COLOR((uint8_t)lroundf(f[0]), (uint8_t)lroundf(f[1]),
                 (uint8_t)lroundf(f[2]));
}

/* ------------------------------------------------------------------------ */
/* the design                                                               */
/* ------------------------------------------------------------------------ */

typedef struct design
{
    float hue;     /* dominant hue */
    float hue2;    /* second hue of the art, < 0: none */
    float vivid;   /* 0..1 */
    float bright;  /* 0..1: how light the picture is overall */
} design;

/* Angles from the dominant hue that sit well with it: analogous either side,
 * split complements, the complement. */
static float harmonic(float hue, float hue2)
{
    static const float angles[] = {30.0f, -30.0f, 150.0f, -150.0f, 180.0f};
    if (hue2 < 0.0f)
        return wrap_hue(hue + 32.0f);
    float d = hue_delta(hue, hue2), best = angles[0], err = 1e9f;
    for (size_t i = 0; i < sizeof(angles) / sizeof(angles[0]); i++)
    {
        float e = fabsf(hue_delta(d, angles[i]));
        if (e < err)
        {
            err = e;
            best = angles[i];
        }
    }
    return wrap_hue(hue + best);
}

static ui_palette compose(const design *d)
{
    float v = MATH_CLAMP(d->vivid, 0.0f, 1.0f);
    float h1 = d->hue, h2 = harmonic(d->hue, d->hue2);

    float l_bg = 0.13f + 0.20f * MATH_CLAMP(d->bright / 0.7f, 0.0f, 1.0f);
    float c_bg = 0.025f + 0.065f * v;
    /* the glow drifts a little towards the second hue */
    float h_glow = wrap_hue(h1 + MATH_CLAMP(hue_delta(h1, h2), -30.0f, 30.0f) * 0.4f);

    ui_palette p;
    p.bg = ui_oklch_to_color((ui_oklch){l_bg, c_bg, h1});
    p.surface = ui_oklch_to_color((ui_oklch){l_bg + 0.045f, c_bg * 1.1f, h1});
    p.glow = ui_oklch_to_color((ui_oklch){l_bg + 0.10f, c_bg * 1.3f + 0.01f, h_glow});
    p.pulse = ui_oklch_to_color((ui_oklch){l_bg + 0.085f, c_bg * 1.3f, h1});
    p.accent = ui_oklch_to_color((ui_oklch){0.80f, 0.07f + 0.09f * v, h1});
    p.accent2 = ui_oklch_to_color((ui_oklch){0.77f, 0.06f + 0.08f * v, h2});
    p.hot = ui_oklch_to_color((ui_oklch){0.92f, 0.05f + 0.05f * v, h2});
    p.text = ui_oklch_to_color((ui_oklch){0.95f, 0.012f, h1});
    p.text_dim = ui_oklch_to_color((ui_oklch){0.74f, 0.03f + 0.01f * v, h1});
    return p;
}

ui_palette ui_palette_from_accent(color_t accent, color_t accent2)
{
    ui_oklch a = ui_color_to_oklch(accent), b = ui_color_to_oklch(accent2);
    design d = {.hue = a.h,
                .hue2 = b.C > 0.02f ? b.h : -1.0f,
                .vivid = a.C / 0.15f,
                .bright = 0.4f};
    return compose(&d);
}

typedef struct bin
{
    float weight;
    float sx, sy; /* weighted unit vectors, to average hues on the circle */
    float chroma; /* weighted chroma */
} bin;

bool ui_palette_extract(const uint8_t *rgb, int w, int h, ui_palette *out)
{
    if (rgb == NULL || out == NULL || w <= 0 || h <= 0)
        return false;

    bin bins[HUE_BINS];
    memset(bins, 0, sizeof(bins));

    int step_x = MATH_MAX(w / SAMPLE_GRID, 1), step_y = MATH_MAX(h / SAMPLE_GRID, 1);
    int total = 0, colourful = 0;
    float sum_l = 0.0f, sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;

    for (int y = 0; y < h; y += step_y)
        for (int x = 0; x < w; x += step_x)
        {
            const uint8_t *px = rgb + ((size_t)y * (size_t)w + (size_t)x) * 3;
            color_t c = COLOR(px[0], px[1], px[2]);
            ui_oklch o = ui_color_to_oklch(c);
            total++;
            sum_l += o.L;
            sum_r += (float)c.r;
            sum_g += (float)c.g;
            sum_b += (float)c.b;
            if (o.C < MIN_CHROMA || o.L < MIN_L || o.L > MAX_L)
                continue;
            colourful++;
            /* a small vivid logo beats a large dull background */
            float wt = o.C * o.C;
            bin *b = &bins[(int)(o.h / HUE_BIN_DEG) % HUE_BINS];
            b->weight += wt;
            b->sx += wt * cosf(o.h * 0.01745329f);
            b->sy += wt * sinf(o.h * 0.01745329f);
            b->chroma += wt * o.C;
        }

    design d = {.hue2 = -1.0f, .bright = sum_l / (float)total};

    if ((float)colourful < MIN_COLOURFUL * (float)total)
    {
        /* no hue to follow: keep a faint tint of what is there, else slate */
        ui_oklch avg = ui_color_to_oklch(COLOR((uint8_t)(sum_r / (float)total),
                                               (uint8_t)(sum_g / (float)total),
                                               (uint8_t)(sum_b / (float)total)));
        d.hue = avg.C > 0.02f ? avg.h : NEUTRAL_HUE;
        d.vivid = MATH_MIN(avg.C / 0.15f, 0.2f);
        *out = compose(&d);
        return true;
    }

    float score[HUE_BINS];
    for (int i = 0; i < HUE_BINS; i++)
        score[i] = bins[i].weight +
                   0.5f * (bins[(i + HUE_BINS - 1) % HUE_BINS].weight +
                           bins[(i + 1) % HUE_BINS].weight);
    int best = 0;
    for (int i = 1; i < HUE_BINS; i++)
        if (score[i] > score[best])
            best = i;
    int second = -1;
    for (int i = 0; i < HUE_BINS; i++)
    {
        int apart = abs(i - best);
        apart = MATH_MIN(apart, HUE_BINS - apart);
        if (apart < HUE_APART || score[i] < SECOND_MIN * score[best])
            continue;
        if (second < 0 || score[i] > score[second])
            second = i;
    }

    /* hue of a bin with its neighbours, averaged on the circle */
    float *hue_out[2] = {&d.hue, &d.hue2};
    int pick[2] = {best, second};
    float vivid = 0.0f;
    for (int k = 0; k < 2; k++)
    {
        if (pick[k] < 0)
            continue;
        float sx = 0, sy = 0, wsum = 0, csum = 0;
        for (int dd = -1; dd <= 1; dd++)
        {
            const bin *b = &bins[(pick[k] + dd + HUE_BINS) % HUE_BINS];
            sx += b->sx;
            sy += b->sy;
            wsum += b->weight;
            csum += b->chroma;
        }
        *hue_out[k] = wrap_hue(atan2f(sy, sx) * 57.29578f);
        if (k == 0 && wsum > 0.0f)
            vivid = (csum / wsum) / 0.15f;
    }
    d.vivid = vivid;
    *out = compose(&d);
    return true;
}

ui_palette ui_palette_mix(const ui_palette *a, const ui_palette *b, float t)
{
    return (ui_palette){
        .bg = color_mix(a->bg, b->bg, t),
        .surface = color_mix(a->surface, b->surface, t),
        .glow = color_mix(a->glow, b->glow, t),
        .pulse = color_mix(a->pulse, b->pulse, t),
        .accent = color_mix(a->accent, b->accent, t),
        .accent2 = color_mix(a->accent2, b->accent2, t),
        .hot = color_mix(a->hot, b->hot, t),
        .text = color_mix(a->text, b->text, t),
        .text_dim = color_mix(a->text_dim, b->text_dim, t),
    };
}
