#ifndef __PLUGIN_H
#define __PLUGIN_H

#include "plugin_api.h"

#ifdef _WIN32
#  define PLUGIN_PATH "~/AppData/Local/aplayer/plugins/"
#elif defined(__linux__)
#  define PLUGIN_PATH "~/.local/share/aplayer/plugins/"
#endif // _WIN32

#define PLUGIN_FN_LIST(DO)                                                     \
    DO(load)                                                                   \
    DO(unload)                                                                 \
    DO(process)                                                                \
    DO(render)                                                                 \
    DO(on_event)                                                               \
    DO(get_info)

#define PLUGIN_SAFECALL(_plugin, fncall, fnclean, arr, i)                      \
    try fncall;                                                                \
    except                                                                     \
    {                                                                          \
        log_error("Plugin '%s' crashed on '%s'\n", (_plugin)->filepath,        \
                  #fncall);                                                    \
        try fnclean;                                                           \
        except_ignore;                                                         \
        plugin_unload(_plugin);                                                \
        array_remove(arr, i, 1);                                               \
        i--;                                                                   \
    }

typedef plugin_ctx *(*plug_load_fn)();
typedef void (*plug_unload_fn)(plugin_ctx *);
typedef void (*plug_process_fn)(plugin_ctx *, plugin_process_param);
typedef void (*plug_render_fn)(plugin_ctx *, plugin_widget_ctx);
typedef void (*plug_on_event_fn)(plugin_ctx *, plugin_widget_ctx, term_event);
typedef const char *(*plug_get_info_fn)(plugin_ctx *);

#define _DO_DEFINE(name) plug_##name##_fn name;
typedef struct plugin_module
{
    PLUGIN_FN_LIST(_DO_DEFINE);
    plugin_ctx *ctx;
    char *filepath;
    void *handle;
} plugin_module;
#define PLUGIN_IDX(arr, index) (((plugin_module *)(arr.data))[index])

plugin_module plugin_load(const char *plugname);
int plugin_unload(plugin_module *plugin);

#endif /* __PLUGIN_H */
