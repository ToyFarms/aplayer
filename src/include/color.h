#ifndef __COLOR_H
#define __COLOR_H

#include <stdint.h>

typedef struct color_t
{
    uint8_t r, g, b, a;
} color_t;

typedef struct color_hsl_t
{
    float h, s, l, a;
} color_hsl_t;

typedef struct color_norm_t
{
    float r, g, b, a;
} color_norm_t;

#define COLOR_NONE             ((color_t){0})
#define COLOR(r, g, b)         ((color_t){(r), (g), (b), 255})
#define COLOR_RGBA(r, g, b, a) ((color_t){(r), (g), (b), (a)})
#define COLOR_GRAY(x)          ((color_t){(x), (x), (x), 255})
#define COLOR_HSL(h, s, l)     ((color_hsl_t){(h), (s), (l), 255.0f})
#define COLOR_HSLA(h, s, l, a) ((color_hsl_t){(h), (s), (l), (a)})
#define COLOR_UNPACK(c)        (c).r, (c).g, (c).b
#define COLOR_UNPACK_RGBA(c)   (c).r, (c).g, (c).b, (c).a
#define COLOR_FMT(c)           #c "(%d, %d, %d, %d)"

color_t color_hex(uint32_t hex);
color_t color_hsl(color_hsl_t hsl);
int color_term_pallete(color_t color);
color_norm_t color_normalize(color_t color);
color_t color_scale(color_norm_t color);

#endif /* __COLOR_H */
