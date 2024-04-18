#include "ap_ui.h"

void ap_widget_init(APWidget *w, Vec2 pos, Vec2 size,
                    void (*draw)(struct APWidget *))
{
    if (!w)
        return;

    w->pos = pos;
    w->size = size;
    w->draw_cmd = sdsempty();
    w->_conv_utf = sdsempty();
    w->draw = draw;
}
