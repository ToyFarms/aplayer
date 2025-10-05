#include "app.h"
#include "audio.h"
#include "audio_mixer.h"
#include "audio_source.h"
#include "clock.h"
#include "ds.h"
#include "logger.h"
#include "pathlib.h"
#include "queue.h"
#include "session.h"
#include "term.h"
#include "term_draw.h"
#include "ui.h"
#include "utils.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    srand(gclock_now_ns());
    if (app_init() < 0)
        return 1;

    app_instance *app = app_get();

    if (path_exists(".session.json"))
    {
        FILE *f = fopen(".session.json", "r");
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        rewind(f);

        char *buf = calloc(size, 1);
        fread(buf, 1, size, f);

        fclose(f);

        session_deserialize(app, buf, size);
    }
    else
    {
        for (int i = 1; i < argc; i++)
            playlist_add(&app->playlist, argv[i]);
    }

    clock_highres_t clock = {0};
    clock_init(&clock);

    queue_t event_queue = queue_create();
    event_queue.free = free;
    play_at_index(app, app->playlist.current_idx);

    while (true)
    {
        term_get_events(&event_queue);
        term_event *e;
        while ((e = queue_pop(&event_queue)))
        {
            switch (e->type)
            {
            case TERM_EVENT_KEY:
                if (e->key.ascii == 'q')
                {
                    free(e);
                    goto exit;
                }
                break;
            case TERM_EVENT_MOUSE:
                app->term.mouse_x = e->mouse.x;
                app->term.mouse_y = e->mouse.y;
                app->term.click[0] = e->mouse.state[0];
                app->term.click[1] = e->mouse.state[1];
                app->term.click[2] = e->mouse.state[2];
                break;
            case TERM_EVENT_RESIZE:
                term_size_update(&app->term);
                app->term.resized = true;
            case TERM_EVENT_UNKNOWN:
                break;
            }

            ui_event(&app->ui, e);

            free(e);
        }

        audio_source *src = &ARR_AS(app->audio->mixer.sources, audio_source)[0];
        if (src->is_finished)
            play_next(app);

        if (app->term.resized)
            term_draw_clear(&app->term.buf);

        ui_render(&app->ui);
        term_write(app->term.buf.buf, app->term.buf.len);
        app->term.buf.len = 0;
        app->term.resized = false;

        clock_throttle(&clock, 60);
    }

exit:
    log_debug("Final cleanup\n");
    queue_free(&event_queue);
    clock_free(&clock);
    app_cleanup();
}
