#ifndef __PLUGIN_API_H
#define __PLUGIN_API_H

#include "ds.h"
#include "term.h"

#include <stdbool.h>

#ifdef _WIN32
#  define PLUGIN_EXPORT __declspec(export)
#elif defined(__linux__)
#  define PLUGIN_EXPORT
#endif

typedef struct plugin_process_param
{
    float *samples;
    int size;
    int nb_channels;
    int sample_rate;
} plugin_process_param;

typedef struct plugin_widget_ctx
{
    int term_width, term_height;
    int x, y;
    int width, height;
    int mouse_x, mouse_y;
    bool is_hovered;
    string_t *buf;
} plugin_widget_ctx;

typedef struct plugin_info
{
    const char *name;
    int ver_major;
    int ver_minor;
    int ver_patch;
} plugin_info;

typedef struct plugin_ctx plugin_ctx;

plugin_ctx *apl_load();
void apl_unload(plugin_ctx *);
void apl_process(plugin_ctx *, plugin_process_param);
void apl_render(plugin_ctx *, plugin_widget_ctx);
void apl_on_event(plugin_ctx *, plugin_widget_ctx, term_event);
plugin_info apl_get_info(plugin_ctx *);

#endif /* __PLUGIN_API_H */
