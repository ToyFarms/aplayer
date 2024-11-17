#ifndef __APL_H
#define __APL_H

#include "apl_api.h"
#include "array.h"

#ifdef _WIN32
#  define APL_PATH "~/AppData/Local/aplayer/ext/audio/"
#elif defined(__linux__)
#  define APL_PATH "~/.local/share/aplayer/ext/audio/"
#endif // _WIN32

#define apl_crashed(inst)                                                      \
    {                                                                          \
        log_error("Audio plugin '%s' crashed\n", (inst).super->filepath);      \
        try(inst).super->destroy((inst).ctx);                                  \
        except{};                                                              \
    }

#define APL_FN_LIST(DO)                                                        \
    DO(create)                                                                 \
    DO(destroy)                                                                \
    DO(process)                                                                \
    DO(render)                                                                 \
    DO(on_event)                                                               \
    DO(get_info)

typedef apl_ctx *(*apl_create_fn)();
typedef void (*apl_destroy_fn)(apl_ctx *);
typedef void (*apl_process_fn)(apl_ctx *, const apl_process_param *);
typedef void (*apl_render_fn)(apl_ctx *, const term_status *);
typedef void (*apl_on_event_fn)(apl_ctx *, const term_status *,
                                const term_event *);
typedef plugin_info (*apl_get_info_fn)();

#define DO_DEFINE_APL_FUNCTION(name) apl_##name##_fn name;
typedef struct apl_class
{
    APL_FN_LIST(DO_DEFINE_APL_FUNCTION);
    char *filepath;
    void *handle;
} apl_class;

typedef struct apl_instance
{
    const apl_class *super;
    apl_ctx *ctx;
} apl_instance;

apl_class apl_class_load(const char *path);
int apl_class_unload(apl_class *class);
void apl_class_loads(const char *path, array(apl_class) * classes_out);
void apl_class_unloads(array(apl_class) * classes);
apl_instance apl_new_instance(apl_class *class);

#endif /* __APL_H */
