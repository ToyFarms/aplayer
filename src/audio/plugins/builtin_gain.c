#include "plugin_api.h"
#include "term_draw.h"

#include <math.h>
#include <stdlib.h>

#define DB_TO_LIN(db)  powf(10.0f, (db) / 20.0f)
#define LIN_TO_DB(lin) (20.0f * log10f((lin)))

typedef struct plugin_ctx
{
    float gain;
} plugin_ctx;

PLUGIN_EXPORT plugin_ctx *apl_load()
{
    plugin_ctx *ctx = malloc(sizeof(plugin_ctx));

    ctx->gain = 0.0f;

    return ctx;
}

PLUGIN_EXPORT void apl_unload(plugin_ctx *ctx)
{
    if (ctx == NULL)
        return;

    free(ctx);
}

PLUGIN_EXPORT void apl_process(plugin_ctx *ctx, plugin_process_param p)
{
    for (int i = 0; i < p.size; i++)
        p.samples[i] *= DB_TO_LIN(ctx->gain);
}

PLUGIN_EXPORT void apl_render(plugin_ctx *ctx, plugin_widget_ctx term)
{
    term_draw_pos(term.buf, VEC(0, 0));
    term_draw_strf(term.buf, "Gain: %d dB", (int)ctx->gain + 1);
}

PLUGIN_EXPORT void apl_on_event(plugin_ctx *ctx, plugin_widget_ctx term,
                                term_event event)
{
    if (event.as.key.virtual == TERM_KEY_UP)
        ctx->gain += 1;
    else if (event.as.key.virtual == TERM_KEY_DOWN)
        ctx->gain -= 1;
}

PLUGIN_EXPORT plugin_info apl_get_info(plugin_ctx *ctx)
{
    return (plugin_info){
        .name = "builtin_gain",
        .ver_major = 1,
        .ver_minor = 0,
        .ver_patch = 0,
    };
}
