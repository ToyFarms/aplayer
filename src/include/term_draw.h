#ifndef __TERM_DRAW_H
#define __TERM_DRAW_H

#include "_math.h"
#include "color.h"
#include "ds.h"

#include <stdbool.h>
#include <stdint.h>

enum term_color_mode
{
    TERM_COLOR_24BIT,
    TERM_COLOR_256,
    TERM_COLOR_MONO,
};

void term_color_mode(enum term_color_mode mode);
enum term_color_mode term_get_color_mode();
string_t *term_draw_pos(string_t *buf, vec2 pos);
string_t *term_draw_move(string_t *buf, vec2 pos);
string_t *term_draw_color(string_t *buf, color_t bg, color_t fg);
string_t *term_draw_str(string_t *buf, const char *str, int len);
string_t *term_draw_strf(string_t *buf, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
string_t *term_draw_padding(string_t *buf, int length);
string_t *term_draw_hline(string_t *buf, int length);
string_t *term_draw_vline(string_t *buf, int length, color_t bg, color_t fg);
string_t *term_draw_hlinef(string_t *buf, float length);
string_t *term_draw_vlinef(string_t *buf, float length);
string_t *term_draw_hblockf(string_t *buf, float x);
string_t *term_draw_vblockf(string_t *buf, float x);
string_t *term_draw_rect(string_t *buf, vec2 size, color_t bg, color_t fg);

#endif /* __TERM_DRAW_H */
