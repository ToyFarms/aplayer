#ifndef __TERM_DRAW_H
#define __TERM_DRAW_H

#include "_math.h"
#include "color.h"
#include "ds.h"
#include "term.h"

#include <stdbool.h>
#include <stdint.h>

void term_color_mode(enum term_color_mode mode);
enum term_color_mode term_get_color_mode();
str_t *term_draw_pos(str_t *buf, vec2 pos);
str_t *term_draw_move(str_t *buf, vec2 pos);
str_t *term_draw_color(str_t *buf, color_t bg, color_t fg);
int term_draw_strwidth(str_t *buf);
size_t term_draw_truncate(str_t *dst, str_t *buf, size_t width);
size_t term_draw_truncate_termchar(str_t *dst, str_t *buf, wchar_t end_char,
                                   size_t width);
str_t *term_draw_reset(str_t *buf);
str_t *term_draw_clear(str_t *buf);
str_t *term_draw_str(str_t *buf, const char *str, int len);
str_t *term_draw_strf(str_t *buf, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
str_t *term_draw_padding(str_t *buf, int length);
str_t *term_draw_hline(str_t *buf, int length);
str_t *term_draw_vline(str_t *buf, int length, color_t bg, color_t fg);
str_t *term_draw_hlinef(str_t *buf, float length, int max_length, color_t line,
                        color_t bg);
str_t *term_draw_vlinef(str_t *buf, float length);
str_t *term_draw_hblockf(str_t *buf, float x);
str_t *term_draw_vblockf(str_t *buf, float x);
str_t *term_draw_rect(str_t *buf, vec2 size, color_t bg, color_t fg);
str_t *term_draw_invert(str_t *buf);

#endif /* __TERM_DRAW_H */
