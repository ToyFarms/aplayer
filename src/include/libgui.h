#ifndef _LIBGUI_H
#define _LIBGUI_H

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <libavutil/log.h>

#include "libhelper.h"

typedef struct GUIState
{
    char *title;
    int width, height;
    bool quit;

    SDL_Window *win;
    SDL_Renderer *renderer;
} GUIState;

int gui_init(char *title, int width, int height);
void gui_event_loop();

#endif /* _LIBGUI_H */ 