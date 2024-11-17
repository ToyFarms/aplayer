#include "color.h"
#include "_math.h"
#include <math.h>
#include <stdlib.h>

color_t color_hex(uint32_t hex)
{
    return (color_t){(uint8_t)(hex >> 24), (uint8_t)(hex >> 16),
                     (uint8_t)(hex >> 8), (uint8_t)(hex >> 0)};
}

color_t color_hsl(color_hsl_t hsl)
{
    hsl.s /= 100.0f;
    hsl.l /= 100.0f;

    float c = (1.0f - fabsf(2.0f * hsl.l - 1.0f)) * hsl.s;
    float x = c * (1.0f - fabsf(fmodf(hsl.h / 60.0f, 2.0f) - 1.0f));

    float r = 0, g = 0, b = 0;
    if (hsl.h >= 0 && hsl.h < 60)
    {
        r = c;
        g = x;
        b = 0;
    }
    else if (hsl.h >= 60 && hsl.h < 120)
    {
        r = x;
        g = c;
        b = 0;
    }
    else if (hsl.h >= 120 && hsl.h < 180)
    {
        r = 0;
        g = c;
        b = x;
    }
    else if (hsl.h >= 180 && hsl.h < 240)
    {
        r = 0;
        g = x;
        b = c;
    }
    else if (hsl.h >= 240 && hsl.h < 300)
    {
        r = x;
        g = 0;
        b = c;
    }
    else if (hsl.h >= 300 && hsl.h < 360)
    {
        r = c;
        g = 0;
        b = x;
    }

    float m = hsl.l - c / 2.0f;
    r = roundf((r + m) * 255.0f);
    g = roundf((g + m) * 255.0f);
    b = roundf((b + m) * 255.0f);

    return (color_t){(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)hsl.a};
}

static const int cube_levels[] = {0, 95, 135, 175, 215, 255};
static const int gray_levels[] = {8,   18,  28,  38,  48,  58,  68,  78,
                                  88,  98,  108, 118, 128, 138, 148, 158,
                                  168, 178, 188, 198, 208, 218, 228, 238};

static float color_dist(color_t a, color_t b)
{
    int rdiff = a.r - b.r;
    int gdiff = a.g - b.g;
    int bdiff = a.b - b.b;
    return (rdiff * rdiff) + (gdiff * gdiff) + (bdiff * bdiff);
}

static int closest_level(int value, const int *levels, int nb_levels)
{
    int smallest = 0;
    for (int i = 1; i < nb_levels; i++)
    {
        if (abs(value - levels[i]) < abs(value - levels[smallest]))
            smallest = i;
    }

    return smallest;
}

int color_term_pallete(color_t col)
{
    int r6 = closest_level(col.r, cube_levels, 6);
    int g6 = closest_level(col.g, cube_levels, 6);
    int b6 = closest_level(col.b, cube_levels, 6);
    int cube_index = 16 + 36 * r6 + 6 * g6 + b6;
    color_t cube_col = COLOR(cube_levels[(cube_index - 16) / 36],
                             cube_levels[((cube_index - 16) % 36) / 6],
                             cube_levels[(cube_index - 16) % 6]);
    int cube_diff = color_dist(cube_col, col);

    int avg = (col.r + col.g + col.b) / 3.0f;
    int gray_level = gray_levels[closest_level(avg, gray_levels, 24)];
    int gray_index = 232 + (gray_level - 8) / 10;
    int gray_diff = color_dist(COLOR_GRAY(gray_levels[gray_index - 232]), col);

    return cube_diff <= gray_diff ? cube_index : gray_index;
}

color_norm_t color_normalize(color_t col)
{
    return (color_norm_t){(float)col.r / 255.0f, (float)col.g / 255.0f,
                          (float)col.b / 255.0f, (float)col.a / 255.0f};
}

color_t color_scale(color_norm_t col)
{
    return (color_t){MATH_CLAMP(roundf(col.r * 255.0f), 0, 255),
                     MATH_CLAMP(roundf(col.g * 255.0f), 0, 255),
                     MATH_CLAMP(roundf(col.b * 255.0f), 0, 255),
                     MATH_CLAMP(roundf(col.a * 255.0f), 0, 255)};
}
