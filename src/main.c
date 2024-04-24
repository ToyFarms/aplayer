#include "ap_os.h"
#include "ap_terminal.h"
#include "ap_tui.h"
#include "ap_ui.h"
#include "ap_utils.h"
#include <pthread.h>
#include "ap_playlist.h"
#include <sys/stat.h>

bool is_path_file(const char *path)
{
   struct stat s;
   if (stat(path, &s) != 0)
       return 0;
    return s.st_mode & S_IFREG;
}

int main(int argc, char **argv)
{
    prepare_app_arguments(&argc, &argv);
    APPlaylist pl;
    ap_playlist_init(&pl);
    assert(pl.groups && pl.sources);
    for (int i = 1; i < argc; i++)
        ap_array_append_resize(pl.sources, &(APSource){strdup(argv[i]), is_path_file(argv[i]), !is_path_file(argv[i])}, 1);

    ap_playlist_load(&pl);

    // TODO: fix memory management

    // serializing
    int size;
    FILE *fd = NULL;
    fopen_s(&fd, "test.txt", "wb");
    if (!fd)
        return -1;

    void *buf = ap_playlist_serialize(&pl, false, &size);
    fwrite(buf, 1, size, fd);
    fclose(fd);

    free(buf);

    ap_playlist_free_member(&pl);

    // deserializing
    APPlaylist pl2;
    ap_playlist_init(&pl2);

    fopen_s(&fd, "test.txt", "rb");
    if (!fd)
        return -1;

    int buf_size;
    buf = file_read("test.txt", true, &buf_size);
    ap_playlist_deserialize(&pl2, buf, buf_size);

    for (int i = 0; i < pl2.groups->len; i++)
    {
        APEntryGroup group = ARR_INDEX(pl2.groups, APEntryGroup *, i);
        printf("%s: %d entry\n", group.id, group.entries->len);
    }

    free(buf);

    ap_playlist_free_member(&pl2);

    return 0;

    // APTermContext *termctx = calloc(1, sizeof(*termctx));
    // if (!termctx)
    //     return -1;
    //
    // termctx->handle_out = ap_term_get_std_handle(HANDLE_STDOUT);
    // termctx->handle_in = ap_term_get_std_handle(HANDLE_STDIN);
    // termctx->size = ap_term_get_size(termctx->handle_out);
    //
    // ap_term_switch_alt_buf(termctx->handle_out);
    //
    // pthread_t tui_tid;
    // pthread_create(&tui_tid, NULL, ap_tui_render_loop, termctx);
    //
    // APTermEvent events[128];
    // int event_read;
    // bool should_close = false;
    // while (!should_close)
    // {
    //     ap_term_get_events(termctx->handle_in, events, 128, &event_read);
    //     for (int i = 0; i < event_read; i++)
    //     {
    //         APTermEvent e = events[i];
    //         if (e.type == AP_TERM_KEY_EVENT)
    //         {
    //             // handle key event
    //             if (e.key_event.ascii_key == 'q')
    //                 should_close = true;
    //         }
    //         else if (e.type == AP_TERM_MOUSE_EVENT)
    //         {
    //             if (e.mouse_event.moved)
    //                 termctx->mouse_pos = VEC(e.mouse_event.x, e.mouse_event.y);
    //         }
    //         else if (e.type == AP_TERM_BUF_EVENT)
    //         {
    //             termctx->size =
    //                 VEC(e.buf_event.to_width, e.buf_event.to_height);
    //             termctx->resized = true;
    //         }
    //     }
    // }
    //
    // ap_term_switch_main_buf(termctx->handle_out);
    //
    // return 0;
}
