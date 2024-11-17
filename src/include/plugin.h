/*
 * Internal function for plugin management
 * */

#ifndef __PLUGIN_H
#define __PLUGIN_H

void *plugin_load(const char *path);
int plugin_unload(void *handle);
void *plugin_loadsym(void *handle, const char *symbol);

#endif /* __PLUGIN_H */
