#include "ap_tui.h"

void *ap_tui_render_loop(void *arg)
{
    APTermContext *termctx = (APTermContext *)arg;
    float fps = 60.0f;

    APArray *widgets = ap_array_alloc(16, sizeof(APWidget));

    APWidget filelist;
    ap_widget_init(&filelist, VEC_ZERO, VEC_ZERO, ap_widget_filelist_draw);
    ap_array_append_resize(widgets, &filelist, 1);

    while (true)
    {
        for (int i = 0; i < widgets->len; i++)
        {
            APWidget *w = ARR_INDEX(widgets, APWidget **, i);
            if (termctx->resized)
            {
                w->size = termctx->size;
                w->resized = true;
            }
            w->draw(w);
        }
        termctx->resized = false;
        av_usleep(MSTOUS(1000.0f / fps));
    }
}
