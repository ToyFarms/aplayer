#include "plugin.h"
#include "logger.h"

#include <dlfcn.h>

void *plugin_load(const char *path)
{
#ifdef _WIN32
#  error Plugin not implemented
#elif defined(__linux__)

    log_debug("Loading module from '%s'\n", path);
    void *handle = dlopen(path, RTLD_NOW);
    if (!handle)
    {
        log_error("Cannot load module: %s\n", dlerror());
        goto exit;
    }

#endif // _WIN32

exit:
    return handle;
}

int plugin_unload(void *handle)
{
#ifdef _WIN32
#  error Plugin not implemented
#elif defined(__linux__)
    log_debug("Unloading module %p\n", handle);
    return dlclose(handle);
#endif // _WIN32
}

void *plugin_loadsym(void *handle, const char *symbol)
{
#ifdef _WIN32
#  error Plugin not implemented
#elif defined(__linux__)
    return dlsym(handle, symbol);
#endif // _WIN32
}
