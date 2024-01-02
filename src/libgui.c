#include "libgui.h"

static GUIState *gui_state_init()
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing GUIState.\n");

    GUIState *gst = (GUIState *)malloc(sizeof(GUIState));
    if (!gst)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate GUIState.\n");
        return NULL;
    }

    memset(gst, 0, sizeof(gst));

    gst->title = NULL;
    gst->width = 0;
    gst->height = 0;
    gst->quit = false;

    gst->win = NULL;
    gst->renderer = NULL;

    return gst;
}

static void gui_state_free(GUIState **gst)
{
    av_log(NULL, AV_LOG_DEBUG, "Free GUIState.\n");

    if (!gst)
        return;

    if (!(*gst))
        return;

    if ((*gst)->win)
    {
        av_log(NULL, AV_LOG_DEBUG, "Free SDL_Window.\n");
        SDL_DestroyWindow((*gst)->win);
    }

    if ((*gst)->renderer)
    {
        av_log(NULL, AV_LOG_DEBUG, "Free SDL_Renderer.\n");
        SDL_DestroyRenderer((*gst)->renderer);
    }

    free(*gst);
    *gst = NULL;
}

static GUIState *gst;

int gui_init(char *title, int width, int height)
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing GUI.\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL. %s.\n", SDL_GetError());
        return -1;
    }

    gst = gui_state_init();

    gst->title = title;
    gst->width = width;
    gst->height = height;

    av_log(NULL, AV_LOG_DEBUG, "Creating SDL_Window.\n");
    gst->win = SDL_CreateWindow(title,
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                width, height,
                                SDL_WINDOW_RESIZABLE);

    if (!gst->win)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not create window. %s.\n", SDL_GetError());
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Creating SDL_Renderer.\n");
    gst->renderer = SDL_CreateRenderer(gst->win, -1, SDL_RENDERER_ACCELERATED);

    if (!gst->renderer)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not create renderer. %s.\n", SDL_GetError());
        return -1;
    }

    return 1;
}

void gui_event_loop()
{
    SDL_Event e;

    while (gst && !gst->quit)
    {
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_QUIT:
                gst->quit = true;
                break;
            default:
                break;
            }
        }

        SDL_SetRenderDrawColor(gst->renderer, 30, 30, 30, 255);
        SDL_RenderClear(gst->renderer);

        SDL_SetRenderDrawColor(gst->renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(gst->renderer, &(SDL_Rect){.x = 200, .y = 200, .w = 100, .h = 100});

        SDL_RenderPresent(gst->renderer);

        SDL_Delay(16);
    }

    gui_state_free(&gst);
    SDL_Quit();
}