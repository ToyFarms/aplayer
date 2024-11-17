#include "apl_api.h"
#include "term_draw.h"

#include <math.h>
#include <stdlib.h>

#define DB_TO_LIN(db)  powf(10.0f, (db) / 20.0f)
#define LIN_TO_DB(lin) (20.0f * log10f((lin)))

typedef struct apl_ctx
{
    float gain;
} apl_ctx;

PLUGIN_EXPORT apl_ctx *apl_create()
{
    apl_ctx *ctx = calloc(1, sizeof(apl_ctx));

    return ctx;
}

PLUGIN_EXPORT void apl_destroy(apl_ctx *ctx)
{
    if (ctx == NULL)
        return;

    free(ctx);
}

PLUGIN_EXPORT void apl_process(apl_ctx *ctx, const apl_process_param *p)
{
    for (int i = 0; i < p->size; i++)
        p->samples[i] *= DB_TO_LIN(ctx->gain);
}

PLUGIN_EXPORT void apl_render(apl_ctx *ctx, const term_status *term)
{
    term_draw_pos(term->buf, VEC(0, 0));
    term_draw_strf(term->buf, "Gain: %d dB", (int)ctx->gain + 1);
}

PLUGIN_EXPORT void apl_on_event(apl_ctx *ctx, const term_status *term,
                                const term_event *event)
{
    if (event->key.virtual == TERM_KEY_UP)
        ctx->gain += 1;
    else if (event->key.virtual == TERM_KEY_DOWN)
        ctx->gain -= 1;
}

PLUGIN_EXPORT plugin_info apl_get_info(apl_ctx *ctx)
{
    return (plugin_info){
        .name = "builtin_gain",
        .ver_major = 1,
        .ver_minor = 0,
        .ver_patch = 0,
    };
}
