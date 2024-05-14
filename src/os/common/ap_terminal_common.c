#include "ap_terminal.h"

void ap_term_switch_main_buf(APHandle h)
{
    ap_term_write(h, ESC TCMD_MAINBUF ESC TCMD_RESETTERM, -1);
}

void ap_term_show_cursor(APHandle h, bool visible)
{
    if (visible == 0)
        ap_term_write(h, ESC TCMD_CURSORHIDE, -1);
    else
        ap_term_write(h, ESC TCMD_CURSORSHOW, -1);
}

void ap_term_set_cursor(APHandle h, Vec2 pos)
{
    char buf[64];
    int len = snprintf(buf, 64, ESC TCMD_POSYX, pos.y + 1, pos.x + 1);
    ap_term_write(h, buf, len);
}

