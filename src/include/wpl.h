#ifndef __WPL_H
#define __WPL_H

#include "wpl_api.h"

#ifdef _WIN32
#  define WPL_PATH "~/AppData/Local/aplayer/ext/widget/"
#elif defined(__linux__)
#  define WPL_PATH "~/.local/share/aplayer/ext/widget/"
#endif // _WIN32

#define wpl_crashed(inst)                                                      \
    {                                                                          \
        log_error("Widget plugin '%s' crashed\n", (inst).super->filepath);     \
        try(inst).super->destroy((inst).ctx);                                  \
        except{};                                                              \
    }

#define WPL_FN_LIST(DO)                                                        \
    DO(create)                                                                 \
    DO(destroy)                                                                \
    DO(render)                                                                 \
    DO(on_event)                                                               \
    DO(get_info)

typedef wpl_ctx *(*wpl_create_fn)();
typedef void (*wpl_destroy_fn)(wpl_ctx *);
typedef void (*wpl_render_fn)(wpl_ctx *, const term_status *,
                              const wpl_definition *);
typedef void (*wpl_on_event_fn)(wpl_ctx *, const term_status *,
                                const term_event *);
typedef plugin_info (*wpl_get_info_fn)();

#define DO_DEFINE_WPL_CLASS(name) wpl_##name##_fn name;
typedef struct wpl_class
{
    WPL_FN_LIST(DO_DEFINE_WPL_CLASS);
    char *filepath;
    void *handle;
} wpl_class;

typedef struct wpl_instance
{
    const wpl_class *super;
    wpl_ctx *ctx;
} wpl_instance;

wpl_class wpl_class_load(const char *path);
int wpl_class_unload(wpl_class *class);
void wpl_class_loads(const char *path, array(wpl_class) *classes_out);
void wpl_class_unloads(dict_t *classes);
wpl_instance wpl_new_instance(wpl_class *class);

#endif /* __WPL_H */
