#ifndef __LIST_H
#define __LIST_H

// ------- DO NOT REMOVE THE FOLLOWING INCLUDES -------
#include "app.h"
#include "term_draw.h"
// ------- DO NOT REMOVE THE FOLLOWING INCLUDES -------
#include "term.h"
#include "ui.h"

void render_list(ui_state *state, vec2 pos, vec2 size);
void render_hprogress(ui_state *state, vec2 pos, vec2 size, float progress);
void render_debug(ui_state *state, vec2 pos, vec2 size);
void render_rect(ui_state *state, vec2 pos, vec2 size, color_t color);
int render_timestamp(ui_state *state, vec2 pos, vec2 size, uint64_t timestamp,
                     uint64_t duration);
int render_volume(ui_state *state, vec2 pos, vec2 size, float gain);
int render_volume_color(ui_state *state, vec2 pos, vec2 size, float gain,
                        color_t bg, color_t fg);
void render_media_control(ui_state *state, vec2 pos, vec2 size);
void render_vu_meter(ui_state *state, vec2 pos, vec2 size);
int vu_meter_get_width(ui_state *state, int height, int nb_channels);
void render_statusline(ui_state *state, vec2 pos, vec2 size);
int tabs_get_width(ui_state *state);
void render_tabs(ui_state *state, vec2 pos, vec2 size);
vec2 art_resolve_size(const image_t *img, enum image_render_method method,
                      vec2 requested, enum ui_art_size_mode mode);
void art_init(ui_state *state, enum image_render_method method);
vec2 render_art_image(ui_state *state, vec2 pos, int image_index, vec2 size,
                      enum ui_art_size_mode size_mode, enum ui_anchor anchor,
                      enum image_render_method method,
                      const term_capability *cap);
void render_art(ui_state *state, vec2 pos, vec2 size,
                enum image_render_method method, const term_capability *cap);
void render_lyrics(ui_state *state, vec2 pos, vec2 size);

#endif /* __LIST_H */
