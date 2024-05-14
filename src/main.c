#include "ap_os.h"
#include "ap_playlist.h"
#include "ap_terminal.h"
#include "ap_tui.h"
#include "ap_ui.h"
#include "ap_utils.h"
#include <pthread.h>
#include "ap_flags.h"

int main(int argc, char **argv)
{
    prepare_app_arguments(&argc, &argv);

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [dir/file, ...]", argv[0]);
        return 1;
    }

    APPlaylist *pl = ap_playlist_alloc();

    for (int i = 1; i < argc; i++)
        ap_array_append_resize(
            pl->sources, &(APSource){argv[i], is_path_file(argv[i]), true}, 1);

    ap_playlist_load(pl);

    APTermContext *termctx = calloc(1, sizeof(*termctx));
    if (!termctx)
        return 1;

    termctx->handle_out = ap_term_get_std_handle(HANDLE_STDOUT);
    termctx->handle_in = ap_term_get_std_handle(HANDLE_STDIN);
    termctx->size = ap_term_get_size(termctx->handle_out);

#ifndef _DEBUG_TUI_OUTPUT
    ap_term_switch_alt_buf(termctx->handle_out);
#endif // _DEBUG_TUI_OUTPUT

    APWidgets *widgets = ap_array_alloc(16, sizeof(APWidget *));

    APWidget *filelist = calloc(1, sizeof(*filelist));
    ap_widget_init(filelist, VEC(1, 1), VEC(69, termctx->size.y - 5),
                   APCOLOR(230, 200, 150, 255), APCOLOR(30, 30, 30, 255), NULL,
                   ap_widget_filelist_draw, ap_widget_filelist_on_event, NULL);
    ap_array_append_resize(widgets, &filelist, 1);

    ap_tui_widgets_init(widgets);

    ap_widget_filelist_init(filelist, pl);

    ap_tui_render_loop_async(termctx, widgets);

    APEvent events[128];
    int event_read;

    while (!termctx->should_close)
    {
        ap_term_get_events(termctx->handle_in, events, 128, &event_read);
        for (int i = 0; i < event_read; i++)
        {
            APEvent e = events[i];
            ap_tui_propagate_event(widgets, e);
            if (e.type == AP_EVENT_KEY)
            {
                if (e.event.key.ascii == 'q')
                    termctx->should_close = true;
            }
            else if (e.type == AP_EVENT_MOUSE)
            {
                if (e.event.mouse.moved)
                    termctx->mouse_pos = VEC(e.event.mouse.x, e.event.mouse.y);
            }
            else if (e.type == AP_EVENT_BUFFER)
            {
                termctx->size =
                    VEC(e.event.buf.width, e.event.buf.height);
                termctx->resized = true;
            }
        }
    }

    ap_term_switch_main_buf(termctx->handle_out);
    ap_playlist_free(&pl);
    ap_tui_widgets_free(widgets);

    return 0;
}
