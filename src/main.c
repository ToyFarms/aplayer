#include "apl.h"
#include "app.h"
#include "arena_allocator.h"
#include "audio.h"
#include "audio_mixer.h"
#include "audio_source.h"
#include "ds.h"
#include "exception.h"
#include "libavutil/log.h"
#include "logger.h"
#include "term.h"
#include "term_draw.h"
#include "ui_manager.h"
#include "ui_parser.h"
#include "wpl.h"

#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// NOTE: Temporary visualization
// static void display_rms(void *ctx, void *userdata)
// {
//     analyzer_rms_ctx *rms = ctx;
//     static float prev_scale[8] = {0};
//     static int activity_color[8] = {0};
//     static float threshold = 1.0f;
//     static float prev_diff[8] = {0};
//
//     vec2 size = term_size();
//     sds cmd = sdsnewlen(NULL, size.x * size.y * 2);
//     int width = size.x / 2;
//     vec2 pos = VEC(5, size.y - rms->nb_channels);
//
//     for (int ch = 0; ch < rms->nb_channels; ch++)
//     {
//         float dbfs =
//             rms->rms[ch] == 0 ? -70.0f : -0.691f + 10.0f *
//             log10f(rms->rms[ch]);
//         float scale = powf(10.0, dbfs / 20.0) * width;
//
//         float diff = fabs(prev_scale[ch] - scale);
//         bool activity = diff > threshold;
//
//         if (activity)
//             activity_color[ch] =
//                 FFMIN(activity_color[ch] + (prev_diff[ch] * 15), 255);
//         else
//             activity_color[ch] =
//                 FFMAX(activity_color[ch] - (prev_diff[ch] * 20), 0);
//
//         cmd = term_draw_pos(cmd, VEC_ADD(pos, VEC(0, ch)));
//         cmd = term_draw_strf(cmd, "%10.1f dBFS  ", dbfs);
//         if (activity_color[ch] > 10)
//             cmd = term_draw_color(cmd, COLOR_GRAYSCALE(activity_color[ch]),
//                                   COLOR_NONE);
//         cmd = term_draw_str(cmd, "  " TESC TRESET "  ", -1);
//
//         cmd = term_draw_color(cmd, COLOR_GRAYSCALE(255), COLOR_NONE);
//         for (int i = 0; i < width; i++)
//         {
//             if (scale - i <= 0)
//             {
//                 cmd = term_draw_str(cmd, TESC TRESET, -1);
//                 cmd = term_draw_padding(cmd, width - i);
//                 break;
//             }
//             else if (scale - i < 1)
//                 cmd = term_draw_hblockf(cmd, scale - i);
//             else
//                 cmd = term_draw_str(cmd, " ", 1);
//         }
//
//         prev_scale[ch] = scale;
//         prev_diff[ch] = diff;
//     }
//
//     term_write(cmd, sdslen(cmd));
//     sdsfree(cmd);
// }
//
// static void display_fft(void *actx, void *userdata)
// { analyzer_fft_ctx *ctx = actx;
//     const float f_min = 10.0f;
//     const float f_max = ctx->sample_rate / 2.0f;
//     vec2 size = term_size();
//     int margin = 10;
//     int nb_bins = size.x - margin;
//     int bin_height = size.y / 2;
//     float bins[nb_bins];
//     static float prev_bins[1024] = {0};
//
//     for (int i = 0; i < nb_bins; i++)
//     {
//         float f_start = f_min * powf(f_max / f_min, (float)i / nb_bins);
//         float f_end = f_min * powf(f_max / f_min, (float)(i + 1) / nb_bins);
//
//         int s_start = ceilf(f_start / f_max * ctx->size);
//         int s_end = MATH_MIN(ceilf(f_end / f_max * ctx->size), ctx->size -
//         1);
//
//         if (s_end <= s_start)
//             s_end = s_start + 1;
//
//         double sum = 0.0;
//         for (int j = s_start; j < s_end; j++)
//             sum += fabs(ctx->freqs[j]);
//
//         bins[i] = sum / (s_end - s_start + 1);
//     }
//
//     for (int i = 0; i < nb_bins; i++)
//         bins[i] = (MATH_MIN(bins[i] / 100.0f, 1.0f)) * bin_height;
//
//     sds cmd = sdsnewlen(NULL, size.x * size.y * 2);
//
//     for (int height = 0; height < bin_height; height++)
//     {
//         for (int i = 0; i < nb_bins; i++)
//         {
//             vec2 start = VEC(margin / 2, size.y - 4);
//             if (bins[i] > height)
//             {
//                 if (prev_bins[i] - 1 > height)
//                     continue;
//                 cmd = term_draw_pos(cmd, VEC_ADD(start, VEC(i, -height)));
//                 int color = 255 - ((float)height / (float)bin_height) *
//                 255.0f;
//
//                 if (bins[i] - height < 1)
//                 {
//                     cmd = term_draw_color(cmd, COLOR_NONE,
//                                           COLOR(255 - color, color, 0, 255));
//                     cmd = term_draw_vblockf(cmd, bins[i] - (int)bins[i]);
//                     cmd = term_draw_str(cmd, TESC TRESET, -1);
//                 }
//                 else
//                 {
//                     cmd = term_draw_color(
//                         cmd, COLOR(255 - color, color, 0, 255), COLOR_NONE);
//                     cmd = term_draw_str(cmd, " " TESC TRESET, -1);
//                 }
//             }
//             else if (prev_bins[i] > height)
//             {
//                 // clear the previous frame
//                 cmd = term_draw_pos(cmd, VEC_ADD(start, VEC(i, -height)));
//                 cmd = term_draw_str(cmd, " ", -1);
//             }
//         }
//     }
//
//     memcpy(prev_bins, bins,
//            MATH_MIN(nb_bins, sizeof(prev_bins) / sizeof(prev_bins[0])) *
//                sizeof(bins[0]));
//
//     term_write(cmd, sdslen(cmd));
//     sdsfree(cmd);
// }

// static void reload_plugins(array_t *audio, array_t *widget)
// {
//     apl_class_unloads(audio);
//     wpl_class_unloads(widget);
//
//     apl_class_loads(APL_PATH, audio);
//     wpl_class_loads(WPL_PATH, widget);
// }

int main(int argc, char **argv)
{
    logger_set_level(LOG_DEBUG);
    logger_add_output(-1, stdout, LOG_USE_COLOR);
    logger_add_output(-1, fopen("out.log", "r"), LOG_DEFER_CLOSE);

    if (app_init() < 0)
        return 1;

    char *source;
    {
        FILE *f = fopen("input.txt", "r");

        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);

        source = malloc(size + 1);
        source[size] = '\0';
        fread(source, 1, size, f);

        fclose(f);
    }

    arena_allocator arena = arena_create(4096);

    array_t tokens = ui_tokenize(source, &arena);
    // ui_tokens_print(&tokens);
    ui_scene scene = ui_scene_from_tokens(&tokens, &arena);
    array_free(&tokens);

    ui_element_print(&scene.body);

    ui_element_free(&scene.body);
    arena_free(&arena);
    free(source);

    return 0;
    if (argc < 2)
    {
        log_fatal("Usage: %s <file_with_audio>\n", argv[0]);
        return 1;
    }

    if (app_init() < 0)
        return 1;
    // TODO: figure out how to get wpl instance

    app_instance *app = app_get();

    audio_source src =
        audio_from_file(argv[1], app->audio->nb_channels,
                        app->audio->sample_rate, app->audio->sample_fmt);
    array_append(&app->audio->mixer.sources, &src, 1);

    apl_instance apl_inst =
        apl_new_instance(&ARR_AS(app->audio_classes, apl_class)[0]);
    array_append(&app->audio->mixer.master_plugins, &apl_inst, 1);

    array(wpl_instance) widgets = array_create(16, sizeof(wpl_instance));
    wpl_instance wpl_inst =
        wpl_new_instance(&ARR_AS(app->widget_classes, wpl_class)[0]);
    array_append(&widgets, &wpl_inst, 1);

    string_t scrbuf = string_alloc(1024);
    term_status term = {
        .buf = &scrbuf,
    };

    term_event events[128];
    while (true)
    {
        int len = term_get_events(events, 128);
        vec2 size = term_size();

        term.width = size.x;
        term.height = size.y;

        for (int i = 0; i < len; i++)
        {
            term_event e = events[i];

            apl_instance apl_inst;
            ARR_FOREACH(app->audio->mixer.master_plugins, apl_inst, i)
            {
                try apl_inst.super->on_event(apl_inst.ctx, &term, &e);
                except
                {
                    apl_crashed(apl_inst);
                    array_remove(&app->audio->mixer.master_plugins, i, 1);
                    i--;
                }
            }

            switch (e.type)
            {
            case TERM_EVENT_KEY:
                if (e.key.ascii == 'q')
                    goto exit;
                break;
            case TERM_EVENT_MOUSE:
                term.mouse_x = e.mouse.x;
                term.mouse_y = e.mouse.y;
                break;
            case TERM_EVENT_RESIZE:
            case TERM_EVENT_UNKNOWN:
                break;
            }
        }

        apl_instance apl_inst;
        ARR_FOREACH(app->audio->mixer.master_plugins, apl_inst, i)
        {
            try apl_inst.super->render(apl_inst.ctx, &term);
            except
            {
                apl_crashed(apl_inst);
                array_remove(&app->audio->mixer.master_plugins, i, 1);
                i--;
            }
        }

        wpl_instance wpl_inst;
        ARR_FOREACH(widgets, wpl_inst, i)
        {
            wpl_definition def = {
                .x = 0,
                .y = 5,
                .w = 50,
                .h = 10,
                .attr = NULL,
                .theme = NULL,
            };

            try wpl_inst.super->render(wpl_inst.ctx, &term, &def);
            except
            {
                wpl_crashed(wpl_inst);
                array_remove(&widgets, i, 1);
                i--;
            };

            term_draw_str(&scrbuf, TESC TRESET, -1);
        }

        term_write(scrbuf.buf, scrbuf.len);
        scrbuf.len = 0;

        usleep(1000 * 10);
    }

exit:
    string_free(&scrbuf);
    app_cleanup();
}
