#ifndef __UI_H
#define __UI_H

// NOTE: this is a temporary "hardcoded" rendering, until i come up with a
// general layout engine

#include "array.h"
#include "dict.h"
#include "image_renderer.h"
#include "term.h"

typedef struct app_instance app_instance;

enum tab_type
{
    TAB_PLAYLIST,
    TAB_VISUAL,
    TAB_METADATA,
    TAB_LEN,
};

static const char *tab_name(enum tab_type tab)
{
    switch (tab)
    {
    case TAB_PLAYLIST:
        return "PLAYLIST";
    case TAB_VISUAL:
        return "VISUAL";
    case TAB_METADATA:
        return "METADATA";
    default:
        return "TAB_UNKNOWN";
    }
}

#define PLAYLIST_LINE_NOT_INITIALIZED (1 << 0)
#define PLAYLIST_LINE_HOVERED         (1 << 1)
#define PLAYLIST_LINE_PLAYING         (1 << 2)
#define PLAYLIST_IS_LINE_NORMAL(x)                                             \
    (!((x) & PLAYLIST_LINE_HOVERED) && !((x) & PLAYLIST_LINE_PLAYING))

enum vu_meter_style
{
    VU_METER_ANALOG_LINE,
    VU_METER_ANALOG_HALF,
    VU_METER_ANALOG_BAR,
};

typedef struct ui_setting
{
    int scrolloff;
    bool debug;
    dict_t theme;

    struct vu_meter
    {
        int right_pad;
        int tick_gap;
        int left_pad;
        int bar_gap;
        int bar_width;
        enum vu_meter_style style;
        bool render_ticks;
        bool render_marks;
        bool render_numeric;
        bool render_separator;
        bool render_peak;
        bool discrete;
        float peak_decay;
    } vu_meter;

    struct tabs
    {
        int gap;
        int padding;
    } tabs;
} ui_setting;

#define GET_THEMECOLOR(state, key)                                             \
    *(color_t *)dict_get(&(state)->opt.theme, (key), &COLOR(255, 0, 0))

typedef struct ui_playlist_state
{
    int viewport_offset;
    int hovered_idx;
    array(int32_t) lines;
    bool redraw;
    bool recenter;
} ui_playlist_state;

typedef struct ui_debug_state
{
    queue_t logs;
    int current_row;
} ui_debug_state;

typedef struct ui_media_control_state
{
    bool prev_hovered, play_hovered, next_hovered;
    uint64_t prev_last, play_last, next_last;
} ui_media_control_state;

typedef struct ui_vu_meter_state
{
    array(float) bars;
    array(float) prev_bars;
    array(float) easing_bars;
    bool initialized;
    int high_start;
    int high_end;
    int mark_red;
    int mark_yellow;
    array(float) peaks;
    array(uint64_t) peak_set;
} ui_vu_meter_state;

typedef struct ui_tabs_state
{
    array(str_t) tabs;
    enum tab_type selected;
} ui_tabs_state;

typedef struct ui_art_image
{
    int ref_index;
    str_t rendered;
    int width;
    int height;
    enum image_render_method method;
} ui_art_image;

typedef struct ui_art_state
{
    array(image_t) images;
    array(ui_art_image) images_state;
    bool initialized;
    enum image_render_method method;
} ui_art_state;

struct ui_overlay_state;

typedef struct ui_overlay_state
{
    struct ui_overlay_state *next;
    int layer;
    str_t content;
} ui_overlay_state;

typedef struct ui_state
{
    term_state *term;
    app_instance *app;

    float progress;
    ui_playlist_state playlist_st;
    ui_debug_state debug_st;
    ui_media_control_state media_ctl_st;
    ui_vu_meter_state vu_meter_st;
    ui_tabs_state tabs_st;
    ui_art_state art_st;
    ui_overlay_state overlay_st;

    ui_setting opt;
} ui_state;

void ui_init(ui_state *state, term_state *term, app_instance *app);
void ui_free(ui_state *state);
void ui_render(ui_state *state);
void ui_event(ui_state *state, term_event *e);

#endif /* __UI_H */
