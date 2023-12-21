#include <stdbool.h>
#include <pthread.h>

#include "libaudio.h"
#include "libhelper.h"
#include "libfile.h"
#include "libcli.h"

static CLIState *cst;

void compute_offset(CLIState *cst)
{
    cst->force_redraw = false;

    if (cst->selected_idx < 0)
    {
        cst->selected_idx = cst->entry_size - 1;
        cst->entry_offset = cst->entry_size - cst->height;
        cst->force_redraw = true;
    }
    else if (cst->selected_idx > cst->entry_size - 1)
    {
        cst->selected_idx = 0;
        cst->entry_offset = 0;
        cst->force_redraw = true;
    }

    int scroll_margin = cst->height > 12 ? 5 : 1;

    if (cst->selected_idx - cst->entry_offset > cst->height - scroll_margin)
    {
        cst->entry_offset = FFMIN(cst->entry_offset + ((cst->selected_idx - cst->entry_offset) - (cst->height - scroll_margin)), cst->entry_size - cst->height);
        cst->force_redraw = true;
    }
    else if (cst->selected_idx - cst->entry_offset < scroll_margin)
    {
        cst->entry_offset = FFMAX(cst->entry_offset - (scroll_margin - (cst->selected_idx - cst->entry_offset)), 0);
        cst->force_redraw = true;
    }
}

void cycle_next()
{
    cst->playing_idx = wrap_around(cst->playing_idx + 1, 0, cst->entry_size);
    cst->selected_idx = wrap_around(cst->playing_idx, 0, cst->entry_size);
    compute_offset(cst);

    cli_draw(cst);
    play(cst->entries[cst->playing_idx]);
}

void cycle_prev()
{
    cst->playing_idx = wrap_around(cst->playing_idx - 1, 0, cst->entry_size);
    cst->selected_idx = wrap_around(cst->playing_idx, 0, cst->entry_size);
    compute_offset(cst);

    cli_draw(cst);
    play(cst->entries[cst->playing_idx]);
}

#ifdef AP_WINDOWS
void *event_thread(void *arg)
{
    av_log(NULL, AV_LOG_DEBUG, "Starting event_thread.\n");
    audio_wait_until_initialized();

    int64_t last_keypress = 0;
    int64_t keypress_cooldown = 0;
    bool keypress = false;
    float volume_incr = 0.05f;
    float volume_max = 2.0f;

    while (true)
    {
        if (av_gettime() - last_keypress < keypress_cooldown)
        {
            av_usleep(keypress_cooldown - (av_gettime() - last_keypress));
            continue;
        }

        if (GetAsyncKeyState(VIRT_MEDIA_STOP) & 0x8001)
        {
            audio_toggle_play();
            keypress = true;
            keypress_cooldown = ms2us(100);
        }
        else if (GetAsyncKeyState(VIRT_MEDIA_NEXT_TRACK) & 0x8001)
        {
            cycle_next();
            keypress = true;
            keypress_cooldown = ms2us(500);
        }
        else if (GetAsyncKeyState(VIRT_MEDIA_PREV_TRACK) & 0x8001)
        {
            cycle_prev();
            keypress = true;
            keypress_cooldown = ms2us(500);
        }

        if (keypress)
        {
            keypress = false;
            last_keypress = av_gettime();
        }
        else
            av_usleep(ms2us(100));
    }

    return 0;
}
#endif // AP_WINDOWS

void play(char *filename)
{
    if (!audio_is_finished())
    {
        audio_exit();
        audio_wait_until_finished();
    }

    audio_start_async(filename);
    audio_wait_until_initialized();
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        av_log(NULL, AV_LOG_FATAL, "Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    av_log_set_level(AV_LOG_DEBUG);

    prepare_app_arguments(&argc, &argv);

    // TODO: Fix line shifted up when resizing (shrink up), causing a "ghost" line that cannot be cleared even after switching to main buffer. (i don't know how to fix this one)
    // TODO: Add more features to the cli
    // TODO: Make a gui

#ifdef AP_WINDOWS
    pthread_t event_thread_id;
    pthread_create(&event_thread_id, NULL, event_thread, NULL);
#endif // AP_WINDOWS

    cst = cli_state_init();
    cst->entries = list_directory(argv[1], &cst->entry_size);
    cli_get_console_size(cst);

    cli_buffer_switch(BUF_ALTERNATE);

    Handle out_main = cli_get_handle(BUF_MAIN);
    cst->out = cli_get_handle(BUF_ALTERNATE);

    if (!cst->out.handle)
        return 1;

    cst->force_redraw = true;
    cli_draw(cst);

    bool exit = false;

    while (!exit)
    {
        Events rec = cli_read_in();
        if (!rec.event)
        {
            exit = true;
            break;
        }

        for (int i = 0; i < rec.event_size; i++)
        {
            Event e = rec.event[i];
            switch (e.type)
            {
            case KEY_EVENT_TYPE:
                KeyEvent ke = e.key_event;

                if (!ke.key_down)
                    break;

                bool selected_idx_changed = false;
                char key = ke.acsii_key;

                if (key == 'q')
                    exit = true;

                else if (key == 'j' || ke.vk_key == VIRT_DOWN)
                {
                    cst->selected_idx += 1;
                    selected_idx_changed = true;
                }
                else if (key == 'k' || ke.vk_key == VIRT_UP)
                {
                    cst->selected_idx -= 1;
                    selected_idx_changed = true;
                }

                if (selected_idx_changed)
                    compute_offset(cst);

                if (ke.vk_key == VIRT_RETURN)
                {
                    if (cst->selected_idx >= 0)
                    {
                        cst->playing_idx = cst->selected_idx;
                        cst->force_redraw = false;
                        cli_draw(cst);
                        play(cst->entries[cst->playing_idx]);
                    }
                }
                else if (ke.vk_key == VIRT_ESCAPE)
                    cst->selected_idx = -1;

                cli_draw(cst);

                break;
            case MOUSE_EVENT_TYPE:
                MouseEvent me = e.mouse_event;

                if (me.scrolled)
                {
                    me.scroll_delta > 0 ? cst->entry_offset-- : cst->entry_offset++;

                    cst->force_redraw = true;
                    if (cst->entry_offset < 0 || cst->entry_offset > cst->entry_size - cst->height)
                        cst->force_redraw = false;

                    cst->entry_offset = FFMIN(FFMAX(cst->entry_offset, 0), cst->entry_size - cst->height);

                    cli_draw(cst);
                }
                else if (me.moved)
                {
                    cst->hovered_idx = cst->entry_offset + me.y;
                    cst->force_redraw = false;
                    cli_draw(cst);
                }

                if (me.state & LEFT_MOUSE_CLICKED)
                    cst->selected_idx = cst->hovered_idx;

                if (me.state & LEFT_MOUSE_CLICKED && me.double_clicked)
                {
                    cst->playing_idx = cst->selected_idx;
                    cst->force_redraw = false;
                    cli_draw(cst);
                    play(cst->entries[cst->playing_idx]);
                }

                cst->force_redraw = false;
                cli_draw(cst);

                break;
            case BUFFER_CHANGED_EVENT_TYPE:
                cli_get_console_size(cst);
                SetConsoleWindowInfo(out_main.handle,
                                     true,
                                     &(SMALL_RECT){0, 0, cst->width - 1, cst->height - 1});
                cst->force_redraw = true;
                cli_draw(cst);

                break;
            default:
                break;
            }
        }
    }

    cli_buffer_switch(BUF_MAIN);
    cli_state_free(&cst);
    return 0;
}