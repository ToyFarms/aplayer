#include "plugin.h"
#include "logger.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  error Plugin not implemented
#elif defined(__linux__)
#  include <dlfcn.h>
#  include <wordexp.h>
#endif // _WIN32

plugin_module plugin_load(const char *plugpath)
{
    plugin_module plugin = {0};

#ifdef _WIN32
#  error Plugin not implemented
#elif defined(__linux__)
    wordexp_t exp;
    wordexp(plugpath, &exp, 0);

#  define LOAD_PLUGIN(name)                                                    \
      log_debug("Loading function 'apl_%s' from '%s'\n", #name, plugpath);     \
      plugin.name = dlsym(handle, "apl_" #name);                               \
      if (plugin.name == NULL)                                                 \
      {                                                                        \
          log_error("Module '%s' does not implement function 'apl_%s'\n",      \
                    exp.we_wordv[0], #name);                                   \
          errno = -ELIBBAD;                                                    \
          goto exit;                                                           \
      }

    log_debug("Loading module from '%s'\n", plugpath);
    void *handle = dlopen(exp.we_wordv[0], RTLD_NOW);
    if (!handle)
    {
        log_error("Cannot load module '%s': %s\n", exp.we_wordv[0], dlerror());
        goto exit;
    }

    plugin.filepath = strdup(exp.we_wordv[0]);
    plugin.handle = handle;

    PLUGIN_FN_LIST(LOAD_PLUGIN);

#endif // _WIN32
exit:
    wordfree(&exp);
    return plugin;
}

int plugin_unload(plugin_module *plugin)
{
    log_debug("Unloading module '%s'\n", plugin->filepath);

#ifdef _WIN32
#  error Plugin not implemented
#elif defined(__linux__)
    return dlclose(plugin->handle);
#endif

    if (plugin->filepath)
        free(plugin->filepath);
    plugin->filepath = NULL;
}
