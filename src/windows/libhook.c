#include "libhook.h"

void *keyboard_hooks(void *arg)
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
            keypress_cooldown = ms2us(500);
        }
        else if (GetAsyncKeyState(VIRT_MEDIA_NEXT_TRACK) & 0x8001)
        {
            cli_playlist_next();
            keypress = true;
            keypress_cooldown = ms2us(500);
        }
        else if (GetAsyncKeyState(VIRT_MEDIA_PREV_TRACK) & 0x8001)
        {
            cli_playlist_prev();
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