#include <stdbool.h>
#include <pthread.h>

#include "libaudio.h"
#include "libhelper.h"

#define ARRLEN(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

void *event_thread(void *arg)
{
    PlayerState *pst = audio_get_state();
    while (true)
    {
        if (av_gettime() - pst->last_keypress < pst->keypress_cooldown)
        {
            Sleep(us2ms(pst->keypress_cooldown - (av_gettime() - pst->last_keypress)));
            continue;
        }

        if (GetAsyncKeyState(VK_END) & 0x8001)
        {
            audio_toggle_play();
            pst->keypress = true;
            pst->keypress_cooldown = ms2us(100);
        }
        else if (GetAsyncKeyState(VK_RIGHT) & 0x8001)
        {
            audio_seek(5000);
            pst->keypress = true;
            pst->keypress_cooldown = ms2us(50);
        }
        else if (GetAsyncKeyState(VK_LEFT) & 0x8001)
        {
            audio_seek(-5000);
            pst->keypress = true;
            pst->keypress_cooldown = ms2us(50);
        }
        else if (GetAsyncKeyState(VK_UP) & 0x8001)
        {
            audio_set_volume(FFMIN(pst->target_volume + pst->volume_incr, 1.0f));
            pst->keypress = true;
            pst->keypress_cooldown = ms2us(100);
        }
        else if (GetAsyncKeyState(VK_DOWN) & 0x8001)
        {
            audio_set_volume(FFMAX(pst->target_volume - pst->volume_incr, 0.0f));
            pst->keypress = true;
            pst->keypress_cooldown = ms2us(100);
        }
        else if (GetAsyncKeyState(VK_HOME) & 0x8001)
        {
            audio_stop();
            pst->keypress = true;
            pst->keypress_cooldown = ms2us(500);
        }

        if (pst->keypress)
        {
            pst->keypress = false;
            pst->last_keypress = av_gettime();
        }
        else
            Sleep(100);
    }

    return 0;
}

void *update_thread(void *arg)
{
    PlayerState *pst = audio_get_state();
    while (true)
    {
        pst->volume = lerpf(pst->volume, pst->target_volume, pst->volume_lerp);

        Sleep(pst->paused ? 100 : 10);
    }
}

int main(int argc, char **argv)
{
    prepare_app_arguments(&argc, &argv);

    av_log_set_level(AV_LOG_DEBUG);

    if (argc < 2)
    {
        av_log(NULL, AV_LOG_FATAL, "Usage: %s [audio_file, ...]\n", argv[0]);
        return 1;
    }

    pthread_t event_thread_id;
    if (pthread_create(&event_thread_id, NULL, event_thread, NULL) != 0)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not create event_thread.\n");
        return 1;
    }

    pthread_t update_thread_id;
    if (pthread_create(&update_thread_id, NULL, update_thread, NULL) != 0)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not create event_thread.\n");
        return 1;
    }

    pthread_t audio_thread = audio_start_async(argv[1]);
    if (audio_thread < 0)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not create audio thread.\n");
        return 1;
    }

    audio_wait_until_finished();

    return 0;
}