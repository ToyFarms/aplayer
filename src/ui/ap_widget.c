#include "ap_widgets.h"

void ap_widget_init(APWidget *w, Vec2 pos, Vec2 size,
                    APColor fg, APColor bg,
                    void (*init)(APWidget *),
                    void (*draw)(APWidget *),
                    void (*on_event)(APWidget *, APEvent),
                    void (*free)(APWidget *))
{
    w->pos = pos;
    w->size = size;
    w->fg = fg;
    w->bg = bg;
    w->init = init;
    w->draw = draw;
    w->on_event = on_event;
    w->free = free;
}
