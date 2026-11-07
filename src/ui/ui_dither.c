#include "ui_dither.h"

#include "_math.h"

#include <math.h>

float ui_dither_noise(int x, int y, int channel)
{
    /* interleaved gradient noise (Jimenez), shifted per channel so the three
     * channels do not step together and tint the contours */
    float px = (float)x + 5.588238f * (float)channel;
    float py = (float)y + 11.243f * (float)channel;
    float t = 0.06711056f * px + 0.00583715f * py;
    t -= floorf(t);
    t = 52.9829189f * t;
    return t - floorf(t);
}

static uint8_t quant(float v, float n)
{
    return (uint8_t)MATH_CLAMP(floorf(v + n), 0.0f, 255.0f);
}

color_t ui_dither_rgb(const float rgb[3], int x, int y)
{
    return COLOR(quant(rgb[0], ui_dither_noise(x, y, 0)),
                 quant(rgb[1], ui_dither_noise(x, y, 1)),
                 quant(rgb[2], ui_dither_noise(x, y, 2)));
}

/* ---- 256 colours --------------------------------------------------------- */

static color_t xterm_color(int idx)
{
    static const uint8_t lv[6] = {0, 95, 135, 175, 215, 255};
    if (idx >= 232)
        return COLOR_GRAY((uint8_t)(8 + 10 * (idx - 232)));
    idx -= 16;
    return COLOR(lv[idx / 36], lv[(idx % 36) / 6], lv[idx % 6]);
}

void ui_dither_256(const float rgb[3], int x, int y, uint32_t *cp, color_t *fg,
                   color_t *bg)
{
    (void)x;
    (void)y;
    /* Mixing two palette colours with glyphs looks like a checkerboard when
     * the neighbours of a dark colour are as far apart as the 256-colour
     * palette's are, which is worse than the banding it would hide. So the
     * nearest colour is used as it is. */
    color_t c = COLOR((uint8_t)MATH_CLAMP(rgb[0] + 0.5f, 0.0f, 255.0f),
                      (uint8_t)MATH_CLAMP(rgb[1] + 0.5f, 0.0f, 255.0f),
                      (uint8_t)MATH_CLAMP(rgb[2] + 0.5f, 0.0f, 255.0f));
    *cp = ' ';
    *fg = COLOR_NONE;
    *bg = xterm_color(color_term_pallete(c));
}
