#ifndef __LIST_H
#define __LIST_H

#include "app.h"
#include "term_draw.h"
#include "ui.h"

void render_list(ui_state *state, vec2 pos, vec2 size);
void render_hprogress(ui_state *state, vec2 pos, vec2 size, float progress);
void render_debug(ui_state *state, vec2 pos, vec2 size);
void render_rect(ui_state *state, vec2 pos, vec2 size, color_t color);
int render_timestamp(ui_state *state, vec2 pos, vec2 size, uint64_t timestamp,
                     uint64_t duration);
int render_volume(ui_state *state, vec2 pos, vec2 size, float gain);
void render_media_control(ui_state *state, vec2 pos, vec2 size);
void render_vu_meter(ui_state *state, vec2 pos, vec2 size);
int vu_meter_get_width(ui_state *state, int height, int nb_channels);
void render_statusline(ui_state *state, vec2 pos, vec2 size);
int tabs_get_width(ui_state *state);
void render_tabs(ui_state *state, vec2 pos, vec2 size);
void render_art(ui_state *state, vec2 pos, vec2 size);

#endif /* __LIST_H */
