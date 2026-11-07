/* aplayer: startup, session restore and the main loop (input -> playback -> UI). */
#include "app.h"
#include "array.h"
#include "audio.h"
#include "audio_effect.h"
#include "audio_mixer.h"
#include "audio_source.h"
#include "clock.h"
#include "ds.h"
#include "logger.h"
#include "pathlib.h"
#include "queue.h"
#include "session.h"
#include "term.h"
#include "ui.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

static void load_session(app_instance *app)
{
    FILE *f = fopen(".session.json", "r");
    if (f == NULL)
        return;
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    rewind(f);

    char *buf = calloc(size + 1, 1);
    if (buf != NULL && fread(buf, 1, size, f) == size)
    {
        session_deserialize(app, buf, size);
    }
    free(buf);
    fclose(f);
}

static void apply_pending_seek(app_instance *app)
{
    if (app->want_to_seek_ms == 0 || app->audio->mixer.sources.length == 0)
        return;

    audio_source *src;
    ARR_FOREACH_BYREF(app->audio->mixer.sources, src, i)
    {
        src->seek(src, app->want_to_seek_ms, SEEK_SET);

        audio_effect *eff;
        ARR_FOREACH_BYREF(src->effects, eff, j)
        {
            if (eff->type == AUDIO_EFF_FADE)
                audio_eff_fade_force_in(eff);
        }
    }
    app->want_to_seek_ms = 0;
}

int main(int argc, char **argv)
{
    srand((unsigned)gclock_now_ns());
    if (app_init() < 0)
        return 1;

    app_instance *app = app_get();

    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
            playlist_add(&app->playlist, argv[i]);
    }
    else if (path_exists(".session.json"))
        load_session(app);

    clock_highres_t clock = {0};
    clock_init(&clock);

    ui_ctx *ui = ui_create(app);
    if (ui == NULL)
        return 1;

    uint32_t tracks_seen = app->audio->mixer.tracks_finished;
    queue_t events = queue_create();
    events.free = free;
    play_at_index(app, app->playlist.current_idx, true);

    while (!ui_quit_requested(ui))
    {
        term_get_events(&events);
        term_event *e;
        while ((e = queue_pop(&events)))
        {
            ui_event(ui, e);
            free(e);
        }

        uint32_t finished = app->audio->mixer.tracks_finished;
        if (finished != tracks_seen)
        {
            tracks_seen = finished;
            play_next(app);
        }

        apply_pending_seek(app);
        ui_frame(ui);

        clock_throttle(&clock, 60);
    }

    log_debug("Final cleanup\n");
    ui_destroy(ui);
    queue_free(&events);
    clock_free(&clock);
    app_cleanup();
    return 0;
}
