#include <stdbool.h>
#include <pthread.h>

#include "libaudio.h"
#include "libhelper.h"
#include "libwindows.h"
#include "libcli.h"

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

        if (GetAsyncKeyState(VK_MEDIA_STOP) & 0x8001)
        {
            audio_toggle_play();
            keypress = true;
            keypress_cooldown = ms2us(100);
        }
        else if (GetAsyncKeyState(VK_VOLUME_UP) & 0x8001)
        {
            audio_set_volume(FFMIN(audio_get_volume() + volume_incr, volume_max));
            keypress = true;
            keypress_cooldown = ms2us(100);
        }
        else if (GetAsyncKeyState(VK_VOLUME_DOWN) & 0x8001)
        {
            audio_set_volume(FFMAX(audio_get_volume() - volume_incr, 0.0f));
            keypress = true;
            keypress_cooldown = ms2us(100);
        }
        else if (GetAsyncKeyState(VK_MEDIA_NEXT_TRACK) & 0x8001)
        {
            audio_exit();
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
    // TODO: Add audio volume normalization
    // TODO: Factor out windows specific code to `src/windows/`

    pthread_t event_thread_id;
    pthread_create(&event_thread_id, NULL, event_thread, NULL);

    CLIState *cst = cli_state_init();
    cst->entries = list_directory(argv[1], &cst->entry_size);
    cli_get_console_size(cst);

    cli_buffer_switch(BUF_ALTERNATE);

    HANDLE out_main = cli_get_handle(BUF_MAIN);
    cst->out = cli_get_handle(BUF_ALTERNATE);

    if (!cst->out)
        return 1;

    cst->force_redraw = true;
    cli_draw(cst);

    unsigned long rec_size;
    bool exit = false;

    while (!exit)
    {
        INPUT_RECORD *rec = cli_read_in(&rec_size);
        if (!rec)
        {
            exit = true;
            break;
        }
        for (int i = 0; i < rec_size; i++)
        {
            INPUT_RECORD e = rec[i];
            switch (e.EventType)
            {
            case KEY_EVENT:
                if (!e.Event.KeyEvent.bKeyDown)
                    break;

                KEY_EVENT_RECORD ke = e.Event.KeyEvent;
                char key = ke.uChar.AsciiChar;

                bool selected_idx_changed = false;

                if (key == 'q')
                    exit = true;
                else if (key == 'j' || ke.wVirtualKeyCode == VK_DOWN)
                {
                    cst->selected_idx += 1;
                    selected_idx_changed = true;
                }
                else if (key == 'k' || ke.wVirtualKeyCode == VK_UP)
                {
                    cst->selected_idx -= 1;
                    selected_idx_changed = true;
                }

                cst->force_redraw = false;
                if (selected_idx_changed)
                {
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

                if (ke.wVirtualKeyCode == VK_RETURN)
                {
                    if (cst->selected_idx >= 0)
                    {
                        cst->playing_idx = cst->selected_idx;
                        cst->force_redraw = false;
                        cli_draw(cst);
                        play(cst->entries[cst->playing_idx]);
                    }
                }
                else if (ke.wVirtualKeyCode == VK_ESCAPE)
                    cst->selected_idx = -1;

                cli_draw(cst);

                break;
            case MOUSE_EVENT:
                MOUSE_EVENT_RECORD me = e.Event.MouseEvent;
                DWORD button = me.dwButtonState;

                if (me.dwEventFlags & MOUSE_WHEELED)
                {
                    GET_WHEEL_DELTA_WPARAM(me.dwButtonState) > 0 ? cst->entry_offset-- : cst->entry_offset++;

                    cst->force_redraw = true;
                    if (cst->entry_offset < 0 || cst->entry_offset > cst->entry_size - cst->height)
                        cst->force_redraw = false;

                    cst->entry_offset = FFMIN(FFMAX(cst->entry_offset, 0), cst->entry_size - cst->height);

                    cli_draw(cst);
                }
                else if (me.dwEventFlags & MOUSE_MOVED)
                {
                    cst->hovered_idx = cst->entry_offset + me.dwMousePosition.Y;
                    cst->force_redraw = false;
                    cli_draw(cst);
                }

                if (button & FROM_LEFT_1ST_BUTTON_PRESSED)
                    cst->selected_idx = cst->hovered_idx;
                if (button & FROM_LEFT_1ST_BUTTON_PRESSED && me.dwEventFlags == DOUBLE_CLICK)
                {
                    cst->playing_idx = cst->selected_idx;
                    cst->force_redraw = false;
                    cli_draw(cst);
                    play(cst->entries[cst->playing_idx]);
                }

                cst->force_redraw = false;
                cli_draw(cst);

                break;
            case WINDOW_BUFFER_SIZE_EVENT:
                cli_get_console_size(cst);
                SetConsoleWindowInfo(out_main,
                                     true,
                                     &(SMALL_RECT){0, 0, cst->width - 1, cst->height - 1});
                cst->force_redraw = true;
                cli_draw(cst);

                break;
            case FOCUS_EVENT:
                break;
            case MENU_EVENT:
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