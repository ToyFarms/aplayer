#include "ui.h"
#include "_math.h"
#include "app.h"
#include "audio_source.h"
#include "clock.h"
#include "color.h"
#include "image.h"
#include "image_renderer.h"
#include "term_draw.h"
#include "utils.h"
#include "widgets/widgets.h"
#include <stdlib.h>
#include <string.h>

static ui_setting ui_default_setting()
{
    ui_setting s = (ui_setting){
        .scrolloff = 2,
        .debug = false,
        .theme = dict_create(),
    };
    s.theme.strcmp = strcasecmp;

    typedef struct kv_pair
    {
        const char *k;
        color_t v;
    } kv_pair;

    kv_pair themes[] = {
        {"CONTROL_BG", COLOR(10, 10, 10)},
        {"LIST_NORMAL_BG", COLOR(30, 30, 30)},
        {"LIST_NORMAL_FG", COLOR_NONE},
        {"LIST_PLAYING_BG", COLOR(91, 201, 77)},
        {"LIST_PLAYING_FG", COLOR(30, 30, 30)},
        {"LIST_NUMBER_BG", COLOR(10, 10, 10)},
        {"LIST_NUMBER_FG", COLOR(230, 200, 150)},

        {"VOLUME_BG", COLOR(10, 10, 10)},
        {"VOLUME_FG", COLOR(230, 200, 150)},
        {"TIMESTAMP_BG", COLOR(10, 10, 10)},
        {"TIMESTAMP_FG", COLOR(230, 200, 150)},
        {"PROGRESS_BG", COLOR(30, 30, 30)},
        {"PROGRESS_FG", COLOR(255, 0, 0)},
        {"MEDIA_CONTROL_BG", COLOR(255, 255, 255)},
        {"MEDIA_CONTROL_FG", COLOR(0, 0, 0)},
        {"STATUSLINE_BG", COLOR(10, 10, 10)},
        {"STATUSLINE_FG", COLOR(230, 200, 150)},

        {"VU_METER_LOW_FG", COLOR(0, 255, 0)},
        {"VU_METER_MID_FG", COLOR(255, 255, 0)},
        {"VU_METER_HIGH_FG", COLOR(255, 0, 0)},
        {"VU_METER_LOW_BG", COLOR(0, 100, 0)},
        {"VU_METER_MID_BG", COLOR(100, 100, 0)},
        {"VU_METER_HIGH_BG", COLOR(100, 0, 0)},
        {"VU_METER_SEP", COLOR(10, 10, 10)},
        {"VU_METER_BG", COLOR(10, 10, 10)},
        {"VU_METER_FG", COLOR(230, 200, 150)},
    };

    for (int i = 0; i < sizeof(themes) / sizeof(themes[0]); i++)
        dict_insert_copy(&s.theme, themes[i].k, &themes[i].v,
                         sizeof(themes[i].v));

    s.vu_meter.bar_gap = 1;
    s.vu_meter.bar_width = 3;
    s.vu_meter.left_pad = 2;
    s.vu_meter.right_pad = 1;
    s.vu_meter.tick_gap = 1;
    s.vu_meter.render_separator = true;
    s.vu_meter.render_ticks = true;
    s.vu_meter.render_marks = true;
    s.vu_meter.render_numeric = true;
    s.vu_meter.render_peak = true;
    s.vu_meter.peak_decay = 0.995;
    s.vu_meter.style = VU_METER_ANALOG_BAR;

    s.tabs.gap = 1;
    s.tabs.padding = 2;

    return s;
}

void ui_init(ui_state *state, term_state *term, app_instance *app)
{
    memset(state, 0, sizeof(*state));
    state->term = term;
    state->app = app;
    state->opt = ui_default_setting();

    state->debug_st.logs = queue_create();
    state->debug_st.logs.free = free;

    state->vu_meter_st.bars = array_create(8, sizeof(float));
    state->vu_meter_st.prev_bars = array_create(8, sizeof(float));
    state->vu_meter_st.easing_bars = array_create(8, sizeof(float));
    state->vu_meter_st.peaks = array_create(8, sizeof(float));
    state->vu_meter_st.peak_set = array_create(8, sizeof(uint64_t));

    state->tabs_st.tabs = array_create(8, sizeof(str_t));
    for (int i = 0; i < TAB_LEN; i++)
    {
        str_t s = str_new(tab_name((enum tab_type)i));
        array_append(&state->tabs_st.tabs, &s, 1);
    }

    state->art_st.images = array_create(8, sizeof(image_t));
    state->art_st.images_state = array_create(8, sizeof(ui_art_image));
    state->art_st.method = IMAGE_RENDER_BRAILLE;
}

void ui_free(ui_state *state)
{
    queue_free(&state->debug_st.logs);
    array_free(&state->playlist_st.lines);
    dict_free(&state->opt.theme);
    array_free(&state->vu_meter_st.bars);
    array_free(&state->vu_meter_st.prev_bars);
    array_free(&state->vu_meter_st.easing_bars);
    array_free(&state->vu_meter_st.peaks);
    array_free(&state->vu_meter_st.peak_set);

    str_t *s;
    ARR_FOREACH_BYREF(state->tabs_st.tabs, s, i)
    {
        str_free(s);
    }
    array_free(&state->tabs_st.tabs);

    image_t *img;
    ARR_FOREACH_BYREF(state->art_st.images, img, i)
    {
        image_free(img);
    }
    array_free(&state->art_st.images);

    ui_art_image *img_state;
    ARR_FOREACH_BYREF(state->art_st.images_state, img_state, i)
    {
        str_free(&img_state->rendered);
    }
    array_free(&state->art_st.images_state);
}

static void ui_update(ui_state *state)
{
    const audio_source *src =
        &ARR_AS(state->app->audio->mixer.sources, audio_source)[0];
    state->progress = (double)src->timestamp / (double)src->duration;
}

typedef struct widget
{
    vec2 pos;
    vec2 size;
} widget;

static void render_playlist_tabs(ui_state *state)
{
    int meter_width = vu_meter_get_width(state, state->term->height - 3,
                                         state->vu_meter_st.bars.length);

    widget list = {.pos = VEC(0, 1),
                   .size = VEC(state->term->width - meter_width, 0)};
    list.size.y = state->term->height - list.pos.y - 3;
    render_list(state, list.pos, list.size);

    int control_mid_y = list.pos.y + list.size.y + 1;

    audio_source src =
        ARR_AS(state->app->audio->mixer.sources, audio_source)[0];
    widget timestamp = {VEC(2, control_mid_y), VEC(0, 1)};
    timestamp.size.x = render_timestamp(state, timestamp.pos, timestamp.size,
                                        src.timestamp, src.duration);
    term_draw_padding(&state->term->buf, 1);
    widget hprogress = {
        VEC(timestamp.pos.x + timestamp.size.x + 1, control_mid_y),
        VEC(state->term->width - (timestamp.pos.x + timestamp.size.x + 12), 1)};

    render_hprogress(state, hprogress.pos, hprogress.size,
                     (double)src.timestamp / (double)src.duration);

    widget volume = {VEC(hprogress.pos.x + hprogress.size.x + 1, control_mid_y),
                     VEC(10, 1)};

    volume.size.x = render_volume(state, volume.pos, volume.size,
                                  state->app->audio->mixer.master_gain);

    widget media_control = {VEC(state->term->width / 2 - 9, control_mid_y + 1),
                            VEC(17, 1)};
    render_media_control(state, media_control.pos, media_control.size);

    widget statusline = {VEC(2, control_mid_y + 1),
                         VEC(media_control.pos.x - 2, 1)};
    render_statusline(state, statusline.pos, statusline.size);

    widget vu_meter = {VEC(state->term->width - meter_width, control_mid_y - 2),
                       VEC(meter_width, list.size.y)};
    render_vu_meter(state, vu_meter.pos, vu_meter.size);

    int tabs_width = tabs_get_width(state);
    widget tabs = {VEC(state->term->width / 2 - tabs_width / 2, 0),
                   VEC(tabs_width, 1)};
    render_tabs(state, tabs.pos, tabs.size);
}

static void render_visual_tabs(ui_state *state)
{
    if (!state->art_st.initialized)
        term_draw_clear(&state->term->buf);

    int width = state->term->width * 0.3;
    int height = state->term->height * 0.5;
    render_art(state,
               VEC(state->term->width / 2 - width / 2,
                   state->term->height / 2 - height / 2),
               VEC(width, height), state->art_st.method);
    // render_art(state, VEC(0, 0), VEC(1, height), state->art_st.method);
}

static void render_metadata_tabs(ui_state *state)
{
}

static void render_overlay(ui_state *state)
{
}

void ui_render(ui_state *state)
{
    ui_update(state);

    switch (state->tabs_st.selected)
    {
    case TAB_PLAYLIST:
        render_playlist_tabs(state);
        break;
    case TAB_VISUAL:
        render_visual_tabs(state);
        break;
    case TAB_METADATA:
        render_metadata_tabs(state);
        break;
    default:
        break;
    }

    render_overlay(state);
}

void ui_event(ui_state *state, term_event *e)
{
    switch (e->type)
    {
    case TERM_EVENT_KEY:
        if (e->key.mod == 0)
        {
            if (e->key.ascii == 'j')
                state->playlist_st.hovered_idx++;
            if (e->key.ascii == 'k')
                state->playlist_st.hovered_idx--;
        }

        if (e->key.virtual == TERM_KEY_DOWN)
            state->playlist_st.hovered_idx++;
        if (e->key.virtual == TERM_KEY_UP)
            state->playlist_st.hovered_idx--;

        if (e->key.ascii == '\n')
        {
            static clock_highres_t clk = {0};
            static uint64_t last_update = 0;

            uint64_t now = clock_now_ns(&clk);
            if (now - last_update < MS2NS(50))
                return;

            play_at_index(state->app, state->playlist_st.hovered_idx);
            last_update = now;
        }
        else if (e->key.ascii == 'g')
        {
            state->playlist_st.hovered_idx = 0;
        }
        else if (e->key.ascii == 'G')
        {
            state->playlist_st.hovered_idx = state->app->playlist.files.length;
        }
        else if (e->key.virtual == TERM_KEY_F3)
        {
            state->opt.debug = !state->opt.debug;
        }
        else if (e->key.ascii == 'j' && e->key.mod & TERM_KMOD_CTRL)
        {
            state->app->audio->mixer.master_gain -= 0.5f;
        }
        else if (e->key.ascii == 'k' && e->key.mod & TERM_KMOD_CTRL)
        {
            state->app->audio->mixer.master_gain += 0.5f;
        }
        else if (e->key.ascii == 'N')
        {
            state->media_ctl_st.next_hovered = true;
            state->media_ctl_st.next_last = gclock_now_ns();
            play_next(state->app);
        }
        else if (e->key.ascii == 'P')
        {
            state->media_ctl_st.prev_hovered = true;
            state->media_ctl_st.prev_last = gclock_now_ns();
            play_prev(state->app);
        }
        else if (e->key.ascii == ' ')
        {
            state->app->audio->mixer.paused = !state->app->audio->mixer.paused;
            state->media_ctl_st.play_hovered = true;
            state->media_ctl_st.play_last = gclock_now_ns();
        }
        else if (e->key.ascii == '{')
        {
            state->playlist_st.hovered_idx = MATH_MAX(
                state->playlist_st.hovered_idx - state->term->height * 0.5, 0);
        }
        else if (e->key.ascii == '}')
        {
            state->playlist_st.hovered_idx = MATH_MIN(
                state->playlist_st.hovered_idx + state->term->height * 0.5,
                state->app->playlist.files.length);
        }
        else if (e->key.virtual == TERM_KEY_LEFT)
        {
            pthread_mutex_lock(&state->app->audio->mixer.source_mutex);
            audio_source *src;
            ARR_FOREACH_BYREF(state->app->audio->mixer.sources, src, i)
            {
                src->seek(src, -2500, SEEK_CUR);
            }
            pthread_mutex_unlock(&state->app->audio->mixer.source_mutex);
        }
        else if (e->key.virtual == TERM_KEY_RIGHT)
        {
            pthread_mutex_lock(&state->app->audio->mixer.source_mutex);
            audio_source *src;
            ARR_FOREACH_BYREF(state->app->audio->mixer.sources, src, i)
            {
                src->seek(src, 2500, SEEK_CUR);
            }
            pthread_mutex_unlock(&state->app->audio->mixer.source_mutex);
        }
        else if (e->key.ascii == 'r')
        {
            playlist_shuffle(&state->app->playlist);
            state->playlist_st.redraw = true;
            state->playlist_st.recenter = true;
            state->playlist_st.hovered_idx = state->app->playlist.current_idx;
        }
        else if (e->key.ascii == 's')
        {
            enum playlist_sort next_sort =
                state->app->playlist.is_shuffled
                    ? state->app->playlist.sort
                    : (state->app->playlist.sort + 1) % PLAYLIST_SORT_LENGTH;
            playlist_sort(&state->app->playlist, next_sort,
                          state->app->playlist.sort_direction);
            state->playlist_st.redraw = true;
            state->playlist_st.recenter = true;
            state->playlist_st.hovered_idx = state->app->playlist.current_idx;
        }
        else if (e->key.ascii == 'd')
        {
            enum playlist_sort_direction dir =
                (state->app->playlist.sort_direction + 1) % 2;
            playlist_sort(&state->app->playlist, state->app->playlist.sort,
                          dir);
            state->playlist_st.redraw = true;
            state->playlist_st.recenter = true;
            state->playlist_st.hovered_idx = state->app->playlist.current_idx;
        }
        else if (e->key.ascii == 'l' && e->key.mod & TERM_KMOD_CTRL)
        {
            term_draw_clear(&state->term->buf);
        }
        else if (e->key.ascii >= '0' && e->key.ascii <= '9')
        {
            if (MATH_INBETWEEN(e->key.ascii - '0' - 1, 0, TAB_LEN - 1) &&
                e->key.ascii - '0' - 1 != (int)state->tabs_st.selected)
            {
                state->tabs_st.selected = e->key.ascii - '0' - 1;
                state->term->resized = true;
            }
        }
        else if (e->key.virtual == TERM_KEY_F1)
        {
            state->art_st.method =
                (state->art_st.method + 1) % IMAGE_RENDER_LENGTH;
        }
        break;
    case TERM_EVENT_MOUSE:
        break;
    case TERM_EVENT_RESIZE:
        break;
    case TERM_EVENT_UNKNOWN:
        break;
    }
}
