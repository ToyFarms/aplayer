#ifndef __TERM_DRAW_H
#define __TERM_DRAW_H

#include "_math.h"
#include "ds.h"

#include <stdint.h>

typedef struct color_t
{
    uint8_t r, g, b, a;
} color_t;

#define COLOR(r, g, b, a)  ((color_t){(r), (g), (b), (a)})
#define COLOR_NONE         ((color_t){0})
#define COLOR_GRAYSCALE(x) ((color_t){(x), (x), (x), 255})
#define COLOR_HEX(hex)                                                         \
    ((color_t){(uint8_t)(hex >> 24), (uint8_t)(hex >> 16),                     \
               (uint8_t)(hex >> 8), (uint8_t)(hex >> 0)})
#define COLOR_UNPACK(c)   (c).r, (c).g, (c).b
#define COLOR_UNPACK_A(c) (c).r, (c).g, (c).b, (c).a
#define COLOR_FMT(c)      #c "(%d, %d, %d, %d)"
#define COLOR_PRINT(c)    printf(COLOR_FMT(c) "\n", COLOR_UNPACK_A(c))

string_t *term_draw_pos(string_t *buf, vec2 pos);
string_t *term_draw_move(string_t *buf, vec2 pos);
string_t *term_draw_color(string_t *buf, color_t bg, color_t fg);
string_t *term_draw_str(string_t *buf, const char *str, int len);
__attribute__((format(printf, 2, 3))) string_t *term_draw_strf(string_t *buf,
                                                         const char *fmt, ...);
string_t *term_draw_padding(string_t *buf, int length);
string_t *term_draw_hline(string_t *buf, int length);
string_t *term_draw_vline(string_t *buf, int length);
string_t *term_draw_hlinef(string_t *buf, float length);
string_t *term_draw_vlinef(string_t *buf, float length);
string_t *term_draw_hblockf(string_t *buf, float x);
string_t *term_draw_vblockf(string_t *buf, float x);
string_t *term_draw_rect(string_t *buf, vec2 size, color_t bg, color_t fg);

#endif /* __TERM_DRAW_H */
