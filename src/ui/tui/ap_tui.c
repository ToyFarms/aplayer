#include "ap_tui.h"

void ap_tui_widgets_init(APArrayT(APWidget) *ws)
{
    for (int i = 0; i < ws->len; i++)
    {
        APWidget *w = ARR_INDEX(ws, APWidget **, i);
        if (w->init)
            w->init(w);

        w->state.tui = calloc(1, sizeof(*w->state.tui));
        w->state.tui->draw_cmd = sdsempty();
    }
}

void ap_tui_widgets_free(APArrayT(APWidget) *ws)
{
    for (int i = 0; i < ws->len; i++)
    {
        APWidget *w = ARR_INDEX(ws, APWidget **, i);
        if (w->free)
            w->free(w);

        sdsfree(w->state.tui->draw_cmd);
        w->state.tui->draw_cmd = NULL;
        free(w->state.tui);
        w->state.tui = NULL;
        free(w);
        w = NULL;
    }

    ap_array_free(&ws);
}

void ap_tui_propagate_event(APArrayT(APWidget) *ws, APEvent e)
{
    for (int i = 0; i < ws->len; i++)
    {
        APWidget *w = ARR_INDEX(ws, APWidget **, i);
        if (w->on_event)
            w->on_event(w, e);
    }
}

void *ap_tui_render_loop(void *arg)
{
    APTUIParams *params = arg;
    APTermContext *termctx = params->termctx;
    APArrayT(APWidget) *widgets = params->widgets;
    float fps = 60.0f;

    while (!termctx->should_close)
    {
        for (int i = 0; i < widgets->len; i++)
        {
            APWidget *w = ARR_INDEX(widgets, APWidget **, i);
            w->resized = !termctx->resized;

            if (w->draw != NULL)
                w->draw(w);

            if (!w->state.tui->draw_cmd || sdslen(w->state.tui->draw_cmd) == 0)
                continue;

            int len;
            wchar_t *out = mbstowchar(w->state.tui->draw_cmd,
                                      sdslen(w->state.tui->draw_cmd), &len);
#ifdef _DEBUG_TUI_OUTPUT
            for (int i = 0; i < len; i++)
            {
                wchar_t c = out[i];
                if (c == L'\x1b' || c == L'\e' || c == L'\033')
                    ap_term_write(termctx->handle_out, "^[", -1);
                else
                    ap_term_writew(termctx->handle_out, &c, 1);
            }
#else
            ap_term_writew(termctx->handle_out, out, len);
#endif // _DEBUG_TUI_OUTPUT

            free(out);
            sdsclear(w->state.tui->draw_cmd);
        }

        termctx->resized = false;
        av_usleep(MSTOUS(1000.0f / (float)fps));
    }

    free(arg);
    return NULL;
}

pthread_t ap_tui_render_loop_async(APTermContext *termctx, APArrayT(APWidget) *widgets)
{
    APTUIParams *p = malloc(sizeof(*p));
    p->termctx = termctx;
    p->widgets = widgets;

    pthread_t tui_tid;
    pthread_create(&tui_tid, NULL, ap_tui_render_loop, p);
    return tui_tid;
}

