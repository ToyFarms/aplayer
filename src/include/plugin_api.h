/*
 * Shared API for wpl and apl plugin
 * */

#ifndef __PLUGIN_API_H
#define __PLUGIN_API_H

#ifdef _WIN32
#  define PLUGIN_EXPORT __declspec(export)
#elif defined(__linux__)
#  define PLUGIN_EXPORT
#endif

typedef struct plugin_info
{
    const char *name;
    int ver_major;
    int ver_minor;
    int ver_patch;
} plugin_info;

#endif /* __PLUGIN_API_H */
