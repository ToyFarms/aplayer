#ifndef __AP_DRAW_H
#define __AP_DRAW_H

#include "ap_math.h"
#include "ap_ui.h"
#include "sds.h"

static const wchar_t *blocks_horizontal = L" ▎▎▍▌▋▊▉█";
static const wchar_t *blocks_vertical = L" ▂▂▄▄▅▆▇█";
static const int block_len = 9;
static const float block_increment = 1.0f / (float)block_len;

sds ap_draw_pos(sds cmd, Vec2 pos);
sds ap_draw_posmove(sds cmd, Vec2 pos);
sds ap_draw_color(sds cmd, APColor bg, APColor fg);
sds ap_draw_str(sds cmd, const char *str, int strlen);
sds ap_draw_strf(sds cmd, const char *fmt, ...);
sds ap_draw_vstrf(sds cmd, const char *fmt, va_list v);
sds ap_draw_strw(sds cmd, const wchar_t *strw, int strwlen);
sds ap_draw_cmd(sds cmd, sds cmd2);
sds ap_draw_reset_attr(sds cmd);
sds ap_draw_padding(sds cmd, int length);
sds ap_draw_hline(sds cmd, int length);
sds ap_draw_vline(sds cmd, int length);
sds ap_draw_rect(sds cmd, Vec2 size);
sds ap_draw_hblockf(sds cmd, float x);
sds ap_draw_vblockf(sds cmd, float x);

#define AP_DRAW_STRC(num, ...) ap_draw_strc##num(__VA_ARGS__)

sds ap_draw_strc1(sds cmd, const char *template, APColor c1);
sds ap_draw_strc2(sds cmd, const char *template, APColor c1, APColor c2);
sds ap_draw_strc3(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3);
sds ap_draw_strc4(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4);
sds ap_draw_strc5(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4, APColor c5);
sds ap_draw_strc6(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4, APColor c5, APColor c6);
sds ap_draw_strc7(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4, APColor c5, APColor c6, APColor c7);
sds ap_draw_strc8(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4, APColor c5, APColor c6, APColor c7,
                  APColor c8);

#endif /* __AP_DRAW_H */
