#include "ap_draw.h"
#include "ap_terminal.h"
#include "ap_utils.h"

sds ap_draw_pos(sds cmd, Vec2 pos)
{
    return sdscatprintf(cmd, ESC TCMD_POSYX, pos.y + 1, pos.x + 1);
}

sds ap_draw_posmove(sds cmd, Vec2 pos)
{
    if (pos.y > 0)
        cmd = sdscatprintf(cmd, ESC TCMD_NDOWN, pos.y);
    else if (pos.y < 0)
        cmd = sdscatprintf(cmd, ESC TCMD_NUP, pos.y * -1);
    if (pos.x > 0)
        cmd = sdscatprintf(cmd, ESC TCMD_NRIGHT, pos.x);
    else if (pos.x < 0)
        cmd = sdscatprintf(cmd, ESC TCMD_NLEFT, pos.x * -1);
    return cmd;
}

sds ap_draw_color(sds cmd, APColor bg, APColor fg)
{
    if (bg.a != 0 && fg.a != 0)
        return sdscatprintf(cmd, ESC TCMD_BGFG, bg.r, bg.g, bg.b, fg.r, fg.g,
                            fg.b);
    else if (bg.a != 0)
        return sdscatprintf(cmd, ESC TCMD_BG, bg.r, bg.g, bg.b);
    else if (fg.a != 0)
        return sdscatprintf(cmd, ESC TCMD_FG, fg.r, fg.g, fg.b);
    else
        return cmd;
}

sds ap_draw_str(sds cmd, const char *str, int strlen)
{
    return strlen > 0 ? sdscatlen(cmd, str, strlen) : sdscat(cmd, str);
}

sds ap_draw_strf(sds cmd, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    cmd = sdscatvprintf(cmd, fmt, args);
    va_end(args);
    return cmd;
}

sds ap_draw_vstrf(sds cmd, const char *fmt, va_list v)
{
    return sdscatvprintf(cmd, fmt, v);
}

sds ap_draw_strw(sds cmd, const wchar_t *strw, int strwlen)
{
    int strlen;
    char *str = wchartombs(strw, strwlen, &strlen);
    cmd = sdscatlen(cmd, str, strlen);
    free(str);
    return cmd;
}

sds ap_draw_cmd(sds cmd, sds cmd2)
{
    return sdscatsds(cmd, cmd2);
}

sds ap_draw_reset_attr(sds cmd)
{
    return sdscat(cmd, ESC TCMD_RESET);
}

sds ap_draw_padding(sds cmd, int length)
{
    if (length <= 0)
        return cmd;

    char *pad = malloc(length);
    memset(pad, ' ', length);

    cmd = sdscatlen(cmd, pad, length);
    free(pad);

    return cmd;
}

sds ap_draw_hline(sds cmd, int length)
{
    if (length <= 0)
        return cmd;
    return sdscatprintf(cmd, ESC TCMD_HLINE, length);
}

sds ap_draw_vline(sds cmd, int length)
{
    for (int i = 0; i < length; i++)
        cmd = sdscat(cmd, " " ESC TCMD_DOWN ESC TCMD_LEFT);
    return cmd;
}

sds ap_draw_rect(sds cmd, Vec2 size)
{
    for (int i = 0; i < size.y; i++)
        cmd = sdscatprintf(cmd, ESC TCMD_HLINE ESC TCMD_DOWN, size.x);
    return cmd;
}

sds ap_draw_hblockf(sds cmd, float x)
{
    x = MATH_CLAMP(x, 0.0f, 1.0f);
    int block_index = (int)(x * (block_len - 1));
    int len;
    char *block = wchartombs(&blocks_horizontal[block_index], 1, &len);

    cmd = sdscatlen(cmd, block, len);
    free(block);

    return cmd;
}

sds ap_draw_strc1(sds cmd, const char *template, APColor c1)
{
    return ap_draw_strf(cmd, template, APCOLOR_UNPACK(c1));
}

sds ap_draw_strc2(sds cmd, const char *template, APColor c1, APColor c2)
{
    return ap_draw_strf(cmd, template, APCOLOR_UNPACK(c1), APCOLOR_UNPACK(c2));
}

sds ap_draw_strc3(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3)
{
    return ap_draw_strf(cmd, template, APCOLOR_UNPACK(c1), APCOLOR_UNPACK(c2),
                        APCOLOR_UNPACK(c3));
}

sds ap_draw_strc4(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4)
{
    return ap_draw_strf(cmd, template, APCOLOR_UNPACK(c1), APCOLOR_UNPACK(c2),
                        APCOLOR_UNPACK(c3), APCOLOR_UNPACK(c4));
}

sds ap_draw_strc5(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4, APColor c5)
{
    return ap_draw_strf(cmd, template, APCOLOR_UNPACK(c1), APCOLOR_UNPACK(c2),
                        APCOLOR_UNPACK(c3), APCOLOR_UNPACK(c4),
                        APCOLOR_UNPACK(c5));
}

sds ap_draw_strc6(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4, APColor c5, APColor c6)
{
    return ap_draw_strf(cmd, template, APCOLOR_UNPACK(c1), APCOLOR_UNPACK(c2),
                        APCOLOR_UNPACK(c3), APCOLOR_UNPACK(c4),
                        APCOLOR_UNPACK(c5), APCOLOR_UNPACK(c6));
}

sds ap_draw_strc7(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4, APColor c5, APColor c6, APColor c7)
{
    return ap_draw_strf(cmd, template, APCOLOR_UNPACK(c1), APCOLOR_UNPACK(c2),
                        APCOLOR_UNPACK(c3), APCOLOR_UNPACK(c4),
                        APCOLOR_UNPACK(c5), APCOLOR_UNPACK(c6),
                        APCOLOR_UNPACK(c7));
}

sds ap_draw_strc8(sds cmd, const char *template, APColor c1, APColor c2,
                  APColor c3, APColor c4, APColor c5, APColor c6, APColor c7,
                  APColor c8)
{
    return ap_draw_strf(cmd, template, APCOLOR_UNPACK(c1), APCOLOR_UNPACK(c2),
                        APCOLOR_UNPACK(c3), APCOLOR_UNPACK(c4),
                        APCOLOR_UNPACK(c5), APCOLOR_UNPACK(c6),
                        APCOLOR_UNPACK(c7), APCOLOR_UNPACK(c8));
}
