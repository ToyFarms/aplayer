#include "array.h"
#include "audio_source.h"
#include "ds.h"
#include "image.h"
#include "image_renderer.h"
#include "term.h"
#include "ui.h"
#include "widgets.h"
#include <libswscale/swscale.h>
#include <string.h>

vec2 art_resolve_size(const image_t *img, enum image_render_method method,
                      vec2 requested, enum ui_art_size_mode mode)
{
    vec2 d = image_pixel_density(method);

    float aspect = ((float)img->width * d.y) / ((float)img->height * d.x);

    int rw = (int)requested.x;
    int rh = (int)requested.y;

    switch (mode)
    {
    case UI_ART_SIZE_AUTO:
        if (rw > 0 && rh <= 0)
            return VEC(rw, (int)lroundf(rw / aspect));

        if (rh > 0 && rw <= 0)
            return VEC((int)lroundf(rh * aspect), rh);

        return requested;

    case UI_ART_SIZE_EXACT:
        return requested;

    case UI_ART_SIZE_WIDTH:
        return VEC(rw, (int)lroundf(rw / aspect));

    case UI_ART_SIZE_HEIGHT:
        return VEC((int)lroundf(rh * aspect), rh);

    case UI_ART_SIZE_CONTAIN:
    case UI_ART_SIZE_COVER: {
        float sx = (float)rw / img->width * d.x;
        float sy = (float)rh / img->height * d.y;

        float s = (mode == UI_ART_SIZE_CONTAIN) ? fminf(sx, sy) : fmaxf(sx, sy);

        return VEC((int)lroundf(img->width * s / d.x),
                   (int)lroundf(img->height * s / d.y));
    }
    }

    return requested;
}

void art_init(ui_state *state, enum image_render_method method)
{
    image_t *img;
    array(image_t) temp_images = array_create(4, sizeof(image_t));

    audio_source *src =
        &ARR_AS(state->app->audio->mixer.sources, audio_source)[0];
    if (src->get_arts)
        src->get_arts(src, &temp_images);

    ui_art_image *ui_img;
    ARR_FOREACH_BYREF(state->art_st.images, ui_img, i)
    {
        image_free(ui_img->img);
        free(ui_img->img);
        str_free(&ui_img->rendered);
    }
    state->art_st.images.length = 0;

    ARR_FOREACH_BYREF(temp_images, img, i)
    {
        image_t *owned = malloc(sizeof(*owned));
        if (owned == NULL)
            continue;

        memcpy(owned, img, sizeof(*owned));

        ui_art_image ui_img = {
            .img = owned,
            .width = img->width,
            .height = img->height,
            .rendered = str_create(),
            .method = method,
        };
        array_append(&state->art_st.images, &ui_img, 1);
    }

    array_free(&temp_images);
    state->art_st.initialized = true;
}

static vec2 anchor_offset(vec2 size, enum ui_anchor anchor)
{
    switch (anchor)
    {
    case UI_ANCHOR_TOP_LEFT:
        return VEC(0, 0);

    case UI_ANCHOR_TOP:
        return VEC(size.x / 2, 0);

    case UI_ANCHOR_TOP_RIGHT:
        return VEC(size.x, 0);

    case UI_ANCHOR_LEFT:
        return VEC(0, size.y / 2);

    case UI_ANCHOR_CENTER:
        return VEC(size.x / 2, size.y / 2);

    case UI_ANCHOR_RIGHT:
        return VEC(size.x, size.y / 2);

    case UI_ANCHOR_BOTTOM_LEFT:
        return VEC(0, size.y);

    case UI_ANCHOR_BOTTOM:
        return VEC(size.x / 2, size.y);

    case UI_ANCHOR_BOTTOM_RIGHT:
        return VEC(size.x, size.y);
    }

    return VEC(0, 0);
}

vec2 render_art_image(ui_state *state, vec2 pos, int image_index, vec2 size,
                      enum ui_art_size_mode size_mode, enum ui_anchor anchor,
                      enum image_render_method method,
                      const term_capability *cap)
{
    if (!state->art_st.initialized)
        art_init(state, method);

    if (image_index < 0 || image_index >= state->art_st.images.length)
        return VEC_ZERO;

    ui_art_image *ui_img =
        &ARR_AS(state->art_st.images, ui_art_image)[image_index];
    vec2 final_size = art_resolve_size(ui_img->img, method, size, size_mode);

    vec2 density = image_pixel_density(method);
    if (ui_img->img->repr == NULL ||
        ui_img->img->repr->width != (int)(final_size.x * density.x) ||
        ui_img->img->repr->height != (int)(final_size.y * density.y))
    {
        image_resize(ui_img->img, SWS_POINT, final_size.x * density.x,
                     final_size.y * density.y);
    }

    if (state->term->resized || ui_img->width != ui_img->img->width ||
        ui_img->height != ui_img->img->height || ui_img->method != method)
    {
        str_t *buf = &state->term->buf;

        ui_img->rendered.len = 0;
        image_render(&ui_img->rendered, ui_img->img->repr, method, cap);
        term_draw_reset(&ui_img->rendered);

        vec2 origin = VEC_SUB(pos, anchor_offset(final_size, anchor));
        term_draw_pos(buf, origin);
        str_cat_str(buf, &ui_img->rendered);

        ui_img->width = ui_img->img->width;
        ui_img->height = ui_img->img->height;
        ui_img->method = method;
    }

    return final_size;
}
