#ifndef __AP_DRAW_H
#define __AP_DRAW_H

#include "sds.h"
#include "ap_terminal.h"
#include "wcwidth.h"
#include "ap_ui.h"
#include "ap_math.h"

static const wchar_t *blocks_horizontal = L" ▎▎▍▌▋▊▉█";
static const wchar_t *blocks_vertical = L" ▂▂▄▄▅▆▇█";
static const int block_len = 9;
static const float block_increment = 1.0f / (float)block_len;

sds ap_draw_pos(sds cmd, Vec2 pos);
sds ap_draw_posmove(sds cmd, Vec2 pos);
sds ap_draw_color(sds cmd, APColor fg, APColor bg);
sds ap_draw_str(sds cmd, const char *str, int strlen);
sds ap_draw_strf(sds cmd, const char *fmt, ...);
sds ap_draw_strw(sds cmd, const wchar_t *strw, int strwlen);
sds ap_draw_cmd(sds cmd, sds cmd2);
sds ap_draw_reset_attr(sds cmd);
sds ap_draw_padding(sds cmd, int length);
sds ap_draw_hline(sds cmd, int length);
sds ap_draw_vline(sds cmd, int length);
sds ap_draw_rect(sds cmd, Vec2 size);
sds ap_draw_hblockf(sds cmd, float x);
sds ap_draw_vblockf(sds cmd, float x);

#endif /* __AP_DRAW_H */
