#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>

#include "libos.h"
#include "libaudio.h"
#include "libhelper.h"
#include "libfile.h"
#include "libcli.h"
#include "libplaylist.h"
#include "libhook.h"

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

    // TODO: Fix audio normalization (sometimes it doesn't work)
    // TODO: Prompt the user for directory if not supplied from the argument
    // TODO: Make the program more stable
    // TODO: Handle non audio file or directory in playlist

    directory = argv[1];

    atexit(cleanup);

    av_log_set_callback(log_callback);
    av_log_set_level(AV_LOG_DEBUG);

    prepare_app_arguments(&argc, &argv);

    pthread_t event_thread_id;
    pthread_create(&event_thread_id, NULL, keyboard_hooks, NULL);

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