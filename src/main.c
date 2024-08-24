#include "audio.h"
#include "audio_mixer.h"
#include "audio_source.h"
#include "ds.h"
#include "exception.h"
#include "fs.h"
#include "libavutil/log.h"
#include "logger.h"
#include "plugin.h"
#include "term.h"

#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
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

int callback(const void *input, void *output, unsigned long frameCount,
             const PaStreamCallbackTimeInfo *timeInfo,
             PaStreamCallbackFlags statusFlags, void *userData)
{
    audio_mixer *mixer = userData;

    float buffer[mixer->sample_rate];
    memset(buffer, 0, sizeof(buffer));

    int ret = mixer_get_frame(mixer, frameCount * mixer->nb_channels, buffer);
    if (ret == EOF)
    {
        log_debug("Audio finished\n");
        return paComplete;
    }
    else if (ret < 0)
    {
        log_error("Failed to get frame from mixer: code=%d\n", ret);
        return paAbort;
    }

    memcpy(output, buffer, frameCount * mixer->nb_channels * sizeof(float));

    return paContinue;
}

static inline void log_av_callback(void *avcl, int level, const char *fmt,
                                   va_list args)
{
    if (level > av_log_get_level())
        return;
    logger_logv(level, "", "ffmpeg", 0, fmt, args);
}

int main(int argc, char **argv)
{
    errno = 0;
    setlocale(LC_ALL, "");
    logger_set_level(LOG_DEBUG);
    logger_add_output(LOG_FATAL, stdout, LOG_USE_COLOR);
    logger_add_output(-1, fopen("out.log", "a"), LOG_DEFER_CLOSE);
    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_callback(log_av_callback);
    exception_unrecoverable(term_mainbuf);

    if (argc < 2)
    {
        log_fatal("Usage: %s <file_with_audio>\n", argv[0]);
        return 1;
    }

    exception_init();

    audio_ctx *audio = audio_create(callback, -1, 2, 48000, AUDIO_FLT);
    // TODO: Implement an ui system
    // TODO: Refactor hard coded value (e.g. audio buffer size)
    // TODO: Refactor mixer & source update logic
    // TODO: Add tests for term_draw, ds
    // TODO: Log error on all function

    // for (int i = 0; i < 20; i++)
    // {
    //     audio_source src = audio_from_file(argv[1], audio->nb_channels,
    //                                        audio->sample_rate,
    //                                        audio->sample_fmt);
    //     array_append(&audio->mixer.sources, &src, 1);
    // }
    audio_source src = audio_from_file(argv[1], audio->nb_channels,
                                       audio->sample_rate, audio->sample_fmt);
    array_append(&audio->mixer.sources, &src, 1);

    array(plugin_module) plugins = array_create(16, sizeof(plugin_module));

    fs_root plugindir = fs_readdir(PLUGIN_PATH, FS_FILTER_DIR);
    for (int i = 0; i < plugindir.len; i++)
    {
        entry_t plugin = plugindir.entries[i];
        int max = plugindir.baselen + plugin.namelen + 2;
        char path[max];
        snprintf(path, max, "%s/%s", plugindir.base, plugin.name);

        plugin_module mod = plugin_load(path);
        if (errno == -ELIBBAD)
            continue;
        PLUGIN_SAFECALL(&mod, mod.ctx = mod.load(), mod.unload(mod.ctx),
                        &audio->mixer.master_plugins, i);

        array_append(&plugins, &mod, 1);
    }
    fs_root_free(&plugindir);

    array_append(&audio->mixer.master_plugins, plugins.data, 1);

    term_altbuf();
    term_event events[128];
    vec2 mouse = VEC_ZERO;
    string_t strbuf = string_alloc(1024);

    while (true)
    {
        int len = term_get_events(events, 128);
        vec2 size = term_size();

        plugin_widget_ctx widget_ctx = {.term_width = size.x,
                                        .term_height = size.y,
                                        .x = 0,
                                        .y = 0,
                                        .width = size.x,
                                        .height = 1,
                                        .mouse_x = mouse.x,
                                        .mouse_y = mouse.y,
                                        .is_hovered =
                                            mouse.x > 0 && mouse.x <= size.x &&
                                            mouse.y > 0 && mouse.y <= 1,
                                        .buf = &strbuf};

        for (int i = 0; i < len; i++)
        {
            term_event e = events[i];

            for (int i = 0; i < audio->mixer.master_plugins.length; i++)
            {
                plugin_module plugin =
                    PLUGIN_IDX(audio->mixer.master_plugins, i);

                PLUGIN_SAFECALL(
                    &plugin, plugin.on_event(plugin.ctx, widget_ctx, e),
                    plugin.unload(plugin.ctx), &audio->mixer.master_plugins, i);
            }

            switch (e.type)
            {
            case TERM_EVENT_KEY:
                if (e.as.key.ascii == 'q')
                    goto exit;
                break;
            case TERM_EVENT_MOUSE:
                mouse = VEC(e.as.mouse.x, e.as.mouse.y);
                break;
            case TERM_EVENT_RESIZE:
            case TERM_EVENT_UNKNOWN:
                break;
            }
        }

        for (int i = 0; i < audio->mixer.master_plugins.length; i++)
        {
            plugin_module plugin = PLUGIN_IDX(audio->mixer.master_plugins, i);

            PLUGIN_SAFECALL(&plugin, plugin.render(plugin.ctx, widget_ctx),
                            plugin.unload(plugin.ctx),
                            &audio->mixer.master_plugins, i);
        }

        term_write(strbuf.buf, strbuf.len);
        strbuf.len = 0;

        usleep(1000 * 10);
    }

exit:
    string_free(&strbuf);
    term_mainbuf();
    audio_free(audio);
    exception_cleanup();
}
