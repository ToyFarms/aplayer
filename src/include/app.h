#ifndef __APP_H
#define __APP_H

#include "audio.h"
#include "ui_manager.h"

typedef struct app_instance
{
    audio_ctx *audio;
    ui_scene scene;
    array(apl_class) audio_classes;
    array(wpl_class) widget_classes;
} app_instance;

int app_init();
app_instance *app_get();
void app_cleanup();

#endif /* __APP_H */
