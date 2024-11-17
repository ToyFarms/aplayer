#ifndef __WPL_API_H
#define __WPL_API_H

#include "dict.h"
#include "plugin_api.h"
#include "term.h"

typedef struct wpl_definition
{
    int x, y, w, h;
    dict_t *theme;
    dict_t *attr;
} wpl_definition;

typedef struct wpl_ctx wpl_ctx;

wpl_ctx *wpl_create();
void wpl_destroy(wpl_ctx *);
void wpl_render(wpl_ctx *, const term_status *, const wpl_definition *);
void wpl_on_event(wpl_ctx *, const term_status *, const term_event *);
plugin_info wpl_get_info();

#endif /* __WPL_API_H */
