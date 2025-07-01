#include "audio_source.h"
#include "image.h"
#include "widgets.h"

void render_art(ui_state *state, vec2 pos, vec2 size)
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
        src->get_arts(src, &state->art_st.images);
        state->art_st.initialized = true;
        state->art_st.renderer.redraw = true;
    }

    if (state->art_st.images.length > 0)
    {
        image_t *img = &ARR_AS(state->art_st.images, image_t)[0];
        if (img && img->repr == NULL)
        {
            float ratio = (float)img->width / (float)img->height;
            image_resize(img, SWS_POINT, size.x, size.x * ratio);
        }
    }

    if (state->art_st.images.length > 0)
    {
        image_t *img = ARR_AS(state->art_st.images, image_t)[0].repr;
        str_t *buf = &state->term->buf;
        if (image_render(&state->art_st.renderer, img,
                         IMAGE_RENDER_HALFBLOCK) ||
            state->term->resized)
        {
            term_draw_pos(buf, pos);
            str_cat_str(buf, &state->art_st.renderer.rendered);
        }

        term_draw_reset(buf);
    }
}
