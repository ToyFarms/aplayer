#include "_math.h"
#include "term_draw.h"
#include "wpl_api.h"
#include <math.h>
#include <stdlib.h>

typedef struct wpl_ctx
{
    int hue;
} wpl_ctx;

wpl_ctx *wpl_create()
{
    wpl_ctx *ctx = malloc(sizeof(*ctx));
    ctx->hue = 0;
    return ctx;
}

void wpl_destroy(wpl_ctx *ctx)
{
    if (ctx == NULL)
        return;

    free(ctx);
}

void wpl_render(wpl_ctx *ctx, const term_status *term,
                const wpl_definition *def)
{
    // term_draw_pos(term->buf, VEC(def->x, def->y));

    ctx->hue = (ctx->hue + 1) % 360;

    // term_color_mode(TERM_COLOR_256);
    for (int j = def->y; j < term->height - def->y; j++)
    {
        term_draw_pos(term->buf, VEC(def->x, j));
        for (int i = 0; i < term->width; i++)
        {
            color_hsl_t hsl = COLOR_HSL((i * 5 + ((i * 8) % j * i) + ctx->hue) % 360, 100, 50);
            color_hsl_t hsl2 = hsl;
            hsl2.h = fmodf(hsl2.h + ctx->hue, 360);
            term_draw_color(term->buf, color_hsl(hsl), color_hsl(hsl2));
            // term_draw_str(term->buf, " ", 1);
            // str_catwch(term->buf, L'▌');
            if ((i + j) % 2 == 0)
                str_catwch(term->buf, L'▞');
            else
                str_catwch(term->buf, L'▚');
        }
    }
}

void wpl_on_event(wpl_ctx *ctx, const term_status *term, const term_event *ev)
{
}

plugin_info wpl_get_info()
{
    return (plugin_info){
        .name = "builtin_list",
        .ver_major = 1,
        .ver_minor = 0,
        .ver_patch = 0,
    };
}
