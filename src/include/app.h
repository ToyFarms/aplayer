#ifndef __APP_H
#define __APP_H

#include "audio.h"
#include "playlist.h"
#include "ui.h"

typedef struct app_instance
{
    audio_ctx *audio;
    playlist_manager playlist;
    ui_state ui;
    term_state term;

    int64_t want_to_seek_ms;
} app_instance;

int app_init();
app_instance *app_get();
void app_cleanup();

#endif /* __APP_H */
