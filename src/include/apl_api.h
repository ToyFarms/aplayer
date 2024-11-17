#ifndef __APL_API_H
#define __APL_API_H

#include "plugin_api.h"
#include "term.h"

typedef struct apl_process_param
{
    float *samples;
    int size;
    int nb_channels;
    int sample_rate;
} apl_process_param;

typedef struct apl_ctx apl_ctx;

apl_ctx *apl_create();
void apl_destroy(apl_ctx *);
void apl_process(apl_ctx *, const apl_process_param *);
void apl_render(apl_ctx *, const term_status *);
void apl_on_event(apl_ctx *, const term_status *, const term_event *);
plugin_info apl_get_info();

#endif /* __APL_API_H */
