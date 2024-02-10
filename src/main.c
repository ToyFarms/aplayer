#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <libgen.h>

#include "libos.h"
#include "libaudio.h"
#include "libhelper.h"
#include "libfile.h"
#include "libcli.h"
#include "libplaylist.h"
#include "libhook.h"

void cleanup(void)
{
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
    prepare_app_arguments(&argc, &argv);

    char *directory = NULL;

    if (argc >= 2)
        directory = argv[1];
    
    // TODO: Fix audio lufs thread not stopping when the main audio switched
    // TODO: Add indicator for audio lufs calculation progress
    
    chdir(dirname(argv[0]));

    atexit(cleanup);

    av_log_set_callback(log_callback);
    av_log_set_level(AV_LOG_DEBUG);

    pthread_t event_thread_id;
    pthread_create(&event_thread_id, NULL, keyboard_hooks, NULL);

    PlayerState *pst = audio_init();
    if (!pst)
        return -1;

    Playlist *pl = playlist_init(directory, pst, !!(directory));
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