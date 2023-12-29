#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>

#include "libos.h"
#include "libaudio.h"
#include "libhelper.h"
#include "libfile.h"
#include "libcli.h"
#include "libplaylist.h"

#ifdef AP_WINDOWS
void *event_thread(void *arg);
#endif // AP_WINDOWS
void *update_thread(void *arg);

void cleanup(void)
{
    cli_buffer_switch(BUF_MAIN);
    FILE *fd = fopen("log.log", "a");

    const char *separator = "========================== END OF LOG ==========================\n\n";
    fwrite(separator, sizeof(char), strlen(separator), fd);

    fclose(fd);
}

void log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    if (level > av_log_get_level())
        return;

    FILE *fd = fopen("log.log", "a");

    char buf[8192];
    vsprintf_s(buf, 8192, fmt, vl);

    fwrite(buf, sizeof(char), strlen(buf), fd);

    fclose(fd);
}

int main(int argc, char **argv)
{
    char *directory;

    if (argc < 2)
    {
        return -1;
    }

    directory = argv[1];

    atexit(cleanup);

    av_log_set_callback(log_callback);
    av_log_set_level(AV_LOG_DEBUG);

    prepare_app_arguments(&argc, &argv);

#ifdef AP_WINDOWS
    pthread_t event_thread_id;
    pthread_create(&event_thread_id, NULL, event_thread, NULL);
#endif // AP_WINDOWS

    PlayerState *pst = audio_init();
    if (!pst)
        return -1;

    Playlist *pl = playlist_init(directory, pst);
    if (!pl)
        return -1;

    if (cli_init(pl) < 0)
        return -1;

    cli_event_loop();

    playlist_free();
    cli_free();
    audio_free();

    return 0;
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
#endif // AP_WINDOWS