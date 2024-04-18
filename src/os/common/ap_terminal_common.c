#include "ap_terminal.h"

void ap_term_switch_main_buf(APHandle h)
{
    const char *seq = ESC "[?1049l";
    ap_term_write(h, seq, strlen(seq));
}

void ap_term_show_cursor(APHandle h, bool visible)
{
    const char *seq_hide = ESC "[?25l";
    const char *seq_show = ESC "[?25h";
    if (visible == 0)
        ap_term_write(h, seq_hide, strlen(seq_hide));
    else
        ap_term_write(h, seq_show, strlen(seq_show));
}

void ap_term_set_cursor(APHandle h, Vec2 pos)
{
    char buf[64];
    int len = snprintf(buf, 64, ESC "[%d;%dH", pos.y, pos.x);
    ap_term_write(h, buf, len);
}

