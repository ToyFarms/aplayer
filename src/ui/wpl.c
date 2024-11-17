#include "wpl.h"
#include "exception.h"
#include "fs.h"
#include "logger.h"
#include "plugin.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

wpl_class wpl_class_load(const char *path)
{
    errno = 0;
    wpl_class class = {0};
    char norm[PATHMAX + 1];
    int len = fs_normpath(path, norm, PATHMAX);
    norm[len] = '\0';

#define LOAD_PLUGIN(name)                                                      \
    log_debug("Loading function 'wpl_%s' from '%s'\n", #name, norm);           \
    class.name = plugin_loadsym(class.handle, "wpl_" #name);                   \
    if (class.name == NULL)                                                    \
    {                                                                          \
        log_error("Module '%s' does not implement function 'wpl_%s'\n", norm,  \
                  #name);                                                      \
        errno = -ELIBBAD;                                                      \
        goto exit;                                                             \
    }

    class.filepath = strdup(norm);
    class.handle = plugin_load(path);

    WPL_FN_LIST(LOAD_PLUGIN);

exit:
    return class;
}

int wpl_class_unload(wpl_class *class)
{
    log_debug("Unloading widget plugin '%s'\n", class->filepath);

    if (class->filepath)
        free(class->filepath);
    class->filepath = NULL;

    return plugin_unload(class->handle);
}

void wpl_class_loads(const char *path, array_t *classes_out)
{
    fs_root dir = fs_readdir(path, FS_FILTER_DIR);
    for (int i = 0; i < dir.len; i++)
    {
        entry_t entry = dir.entries[i];

        int max = dir.baselen + entry.namelen + 32;
        char ppath[max];
        snprintf(ppath, max, "%s/%s", dir.base, entry.name);

        wpl_class class = wpl_class_load(ppath);
        if (errno == -ELIBBAD)
            continue;

        array_append(classes_out, &class, 1);
    }
    fs_root_free(&dir);
}

void wpl_class_unloads(dict_t *classes)
{
    if (classes == NULL)
        return;

    wpl_class *class;
    DICT_FOREACH(key, class, _, classes)
    {
        wpl_class_unload(class);
    }
    DICT_FOREACH_END;

    classes->length = 0;
}

wpl_instance wpl_new_instance(wpl_class *class)
{
    errno = 0;
    wpl_ctx *inst = NULL;
    try inst = class->create();
    except errno = -EINVAL;

    return (wpl_instance){.super = class, .ctx = inst};
}
