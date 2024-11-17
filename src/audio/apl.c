#include "apl.h"
#include "exception.h"
#include "fs.h"
#include "logger.h"
#include "plugin.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

apl_class apl_class_load(const char *path)
{
    errno = 0;
    apl_class class = {0};
    char norm[PATHMAX + 1];
    norm[fs_normpath(path, norm, PATHMAX)] = '\0';

#define LOAD_PLUGIN(name)                                                      \
    log_debug("Loading function 'apl_%s' from '%s'\n", #name, norm);           \
    class.name = plugin_loadsym(class.handle, "apl_" #name);                   \
    if (class.name == NULL)                                                    \
    {                                                                          \
        log_error("Module '%s' does not implement function 'apl_%s'\n", norm,  \
                  #name);                                                      \
        errno = -ELIBBAD;                                                      \
        goto exit;                                                             \
    }

    class.filepath = strdup(norm);
    class.handle = plugin_load(path);

    APL_FN_LIST(LOAD_PLUGIN);

exit:
    return class;
}

int apl_class_unload(apl_class *class)
{
    log_debug("Unloading audio plugin '%s'\n", class->filepath);

    if (class->filepath)
        free(class->filepath);
    class->filepath = NULL;

    return plugin_unload(class->handle);
}

void apl_class_loads(const char *path, array_t *classes_out)
{
    fs_root dir = fs_readdir(path, FS_FILTER_DIR);
    for (int i = 0; i < dir.len; i++)
    {
        entry_t entry = dir.entries[i];

        int max = dir.baselen + entry.namelen + 32;
        char ppath[max];
        snprintf(ppath, max, "%s/%s", dir.base, entry.name);

        apl_class class = apl_class_load(ppath);
        if (errno == -ELIBBAD)
            continue;

        array_append(classes_out, &class, 1);
    }
    fs_root_free(&dir);
}

void apl_class_unloads(array_t *classes)
{
    if (classes == NULL)
        return;

    apl_class *class;
    ARR_FOREACH_BYREF(*classes, class, i)
    {
        apl_class_unload(class);
    }

    classes->length = 0;
}

apl_instance apl_new_instance(apl_class *class)
{
    errno = 0;
    apl_ctx *inst = NULL;
    try inst = class->create();
    except errno = -EINVAL;

    return (apl_instance){.super = class, .ctx = inst};
}
