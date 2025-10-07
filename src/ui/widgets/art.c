#include "audio_source.h"
#include "image.h"
#include "utils.h"
#include "widgets.h"
#include <libswscale/swscale.h>

vec2 art_get_size(ui_state *state, int width, enum image_render_method method)
{
    // TODO: add a way to interface with the image, since its needed for this
    // function, and the callee function
    return VEC_ZERO;
}

void render_art(ui_state *state, vec2 pos, vec2 size,
                enum image_render_method method)
{
    if (!state->art_st.initialized)
    {
        image_t *img;
        ARR_FOREACH_BYREF(state->art_st.images, img, i)
        {
            image_free(img);
        }
        state->art_st.images.length = 0;

        audio_source *src =
            &ARR_AS(state->app->audio->mixer.sources, audio_source)[0];
        if (src->get_arts)
            src->get_arts(src, &state->art_st.images);

        for (int i = state->art_st.images_state.length;
             i < state->art_st.images.length; i++)
            array_append(&state->art_st.images_state,
                         &(ui_art_image){.ref_index = i,
                                         .rendered = str_create(),
                                         .method = method},
                         1);

        ui_art_image *img_state;
        ARR_FOREACH_BYREF(state->art_st.images_state, img_state, i)
        {
            assert(img_state->rendered.buf);
            img_state->width = 0;
            img_state->height = 0;
            img_state->rendered.len = 0;
            img_state->method = -1;
            img_state->ref_index = i;
        }
        state->art_st.images_state.length = state->art_st.images.length;

        state->art_st.initialized = true;
    }

    // TODO: position and size between modes doesn't match

    image_t *img;
    ARR_FOREACH_BYREF(state->art_st.images, img, i)
    {
        vec2 density = image_pixel_density(method);

        int width = size.x * density.x;
        int height = size.y * density.y;

        if (img->repr == NULL || img->repr->width != width ||
            img->repr->height != height)
        {
            image_resize(img, SWS_POINT, width, height);
        }
    }

    ui_art_image *img_state;
    int offset = 0;
    ARR_FOREACH_BYREF(state->art_st.images_state, img_state, i)
    {
        image_t *img =
            ARR_AS(state->art_st.images, image_t)[img_state->ref_index].repr;

        if (state->term->resized || img_state->width != img->width ||
            img_state->height != img->height || img_state->method != method)
        {
            str_t *buf = &state->term->buf;

            img_state->rendered.len = 0;
            image_render(&img_state->rendered, img, method);
            term_draw_reset(&img_state->rendered);

            term_draw_pos(buf, VEC(pos.x + offset, pos.y));
            // print_raw(img_state->rendered.buf);
            str_cat_str(buf, &img_state->rendered);

            img_state->width = img->width;
            img_state->height = img->height;
            img_state->method = method;
        }

        vec2 density = image_pixel_density(method);
        offset += img->width / density.x;
        break;
    }
}
