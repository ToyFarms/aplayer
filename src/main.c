#include "app.h"
#include "array.h"
#include "audio.h"
#include "audio_effect.h"
#include "audio_mixer.h"
#include "audio_source.h"
#include "clock.h"
#include "ds.h"
#include "logger.h"
#include "metagen.h"
#include "pathlib.h"
#include "queue.h"
#include "session.h"
#include "term.h"
#include "term_draw.h"
#include "ui.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void print_opt_double(const char *label, opt_double_t v)
{
    if (v.has_value)
        printf("  %s: %.4f\n", label, v.value);
}

int count_substring(const char *str, const char *sub)
{
    int count = 0;
    int sub_len = strlen(sub);

    if (sub_len == 0)
        return 0;

    const char *ptr = str;

    while ((ptr = strstr(ptr, sub)) != NULL)
    {
        count++;
        ptr += sub_len;
    }
    return count;
}

int main(int argc, char **argv)
{
    srand(gclock_now_ns());
    if (app_init() < 0)
        return 1;

    app_instance *app = app_get();

    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
            playlist_add(&app->playlist, argv[i]);
    }
    else if (path_exists(".session.json"))
    {
        FILE *f = fopen(".session.json", "r");
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        rewind(f);

        char *buf = calloc(size, 1);
        fread(buf, 1, size, f);

        fclose(f);

        session_deserialize(app, buf, size);
        free(buf);
        app->ui.playlist_st.recenter = true;
    }

    clock_highres_t clock = {0};
    clock_init(&clock);

    queue_t event_queue = queue_create();
    event_queue.free = free;
    play_at_index(app, app->playlist.current_idx);
    int want_to_debug = 0;

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
                else if (e->key.virtual == TERM_KEY_F9)
                {
                    want_to_debug = 1;
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

        {
            if (app->want_to_seek_ms != 0 &&
                app->audio->mixer.sources.length > 0)
            {
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
        }

        if (want_to_debug)
        {
            app->term.resized = true;
        }

        ui_render(&app->ui);
        if (want_to_debug)
        {
            term_write(TESC TCLEAR, -1);

            clock_highres_t inner = {0};
            clock_init(&inner);

            str_t status = str_create();
            int i = 0;

            int total = count_substring(app->term.buf.buf, "\x1b[");

            STR_SPLIT(app->term.buf, chunk, "\x1b[")
            {
                term_draw_savepos(&status);
                term_draw_pos(&status, VEC(0, 0));
                term_draw_strf(&status, "%d/%d", i, total);
                term_draw_restorepos(&status);

                i++;

                term_write(status.buf, status.len);
                status.len = 0;

                term_write("\x1b[", -1);
                term_write(chunk.buf, chunk.len);

                clock_throttle(&inner, 120);

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
                        else if (e->key.virtual == TERM_KEY_F9)
                        {
                            want_to_debug = !want_to_debug;
                            if (!want_to_debug)
                                goto out;
                        }
                        break;
                    default:
                        break;
                    }

                    ui_event(&app->ui, e);

                    free(e);
                }
            }

        out:
            clock_free(&inner);
            str_free(&status);

            want_to_debug = 0;
        }
        else
        {
            term_write(app->term.buf.buf, app->term.buf.len);
        }

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
