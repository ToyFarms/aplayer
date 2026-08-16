#include "utils.h"
#include "array.h"
#include "audio_analyzer.h"
#include "audio_effect.h"
#include "audio_source.h"
#include "ds.h"
#include "playlist.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#  include <fcntl.h>
#  include <io.h>
#  define dup_func   _dup
#  define dup2_func  _dup2
#  define open_dev   _open
#  define NUL_PATH   "NUL"
#  define OPEN_FLAGS _O_WRONLY
#else
#  include <fcntl.h>
#  include <unistd.h>
#  define dup_func   dup
#  define dup2_func  dup2
#  define open_dev   open
#  define NUL_PATH   "/dev/null"
#  define OPEN_FLAGS O_WRONLY
#endif

void print_raw(const char *str)
{
    str_t s = str_alloc(strlen(str) * 1.5);

    char c = 0;
    while ((c = *str++))
    {
        switch ((int)c)
        {
        case '\t':
            str_catlen(&s, "\\t", 2);
            break;
        case '\n':
            str_catlen(&s, "\\n", 2);
            break;
        default:
            if (isprint(c))
                str_catch(&s, c);
            else
                str_catf(&s, "\\x%02x", c);
            break;
        }
    }

    log_info("%s", s.buf);
    str_free(&s);
}

static void source_rms_callback(void *actx, void *userdata)
{
    app_instance *app = app_get();
    if (app == NULL)
        return;

    analyzer_rms_ctx *ctx = actx;

    if (app->ui.vu_meter_st.bars.capacity < ctx->nb_channels)
        array_resize(&app->ui.vu_meter_st.bars, ctx->nb_channels);

    app->ui.vu_meter_st.bars.length = ctx->nb_channels;

    for (int i = 0; i < ctx->nb_channels; i++)
        ARR_AS(app->ui.vu_meter_st.bars, float)[i] = ctx->rms[i];
}

static void source_activity_callback(void *actx, void *userdata)
{
    app_instance *app = app_get();
    if (app == NULL)
        return;

    analyzer_activity_ctx *ctx = actx;

    if (app->ui.bands_st.bands.capacity < ctx->nb_channels)
        array_resize(&app->ui.bands_st.bands, ctx->nb_channels);

    app->ui.bands_st.bands.length = ctx->nb_channels;

    for (int i = 0; i < ctx->nb_channels; i++)
        ARR_AS(app->ui.bands_st.bands, float)[i] = ctx->level[i];
}

static void attach_static_effect(audio_source *src)
{
    // TODO: this is hardcoded temporarily, it should be user controlled from a
    // ui
    audio_effect autogain = audio_eff_autogain();
    array_append(&src->effects, &autogain, 1);

    audio_effect fade = audio_eff_fade(1.0, 1.0);
    array_append(&src->effects, &fade, 1);

    audio_analyzer rms = audio_analyzer_rms(source_rms_callback, NULL);
    array_append(&src->analyzer, &rms, 1);

    audio_analyzer activity =
        audio_analyzer_activity(source_activity_callback, NULL);
    array_append(&src->analyzer, &activity, 1);
}

void play_next(app_instance *app)
{
    const fs_entry_t *entry = playlist_next(&app->playlist);
    if (entry == NULL)
        return;

    char *file = entry->path.buf;
    audio_source src = audio_from_file(file);
    if (errno != 0)
    {
        play_next(app);
        log_error("Failed to play %s\n", file);
        return;
    }

    attach_static_effect(&src);
    mixer_clear(&app->audio->mixer);
    array_append(&app->audio->mixer.sources, &src, 1);
    app->ui.playlist_st.hovered_idx = app->playlist.current_idx;
    app->ui.art_st.initialized = false;
}

void play_prev(app_instance *app)
{
    const fs_entry_t *entry = playlist_prev(&app->playlist);
    if (entry == NULL)
        return;

    char *file = entry->path.buf;
    audio_source src = audio_from_file(file);
    if (errno != 0)
    {
        play_prev(app);
        log_error("Failed to play %s\n", file);
        return;
    }

    attach_static_effect(&src);
    mixer_clear(&app->audio->mixer);
    array_append(&app->audio->mixer.sources, &src, 1);
    app->ui.playlist_st.hovered_idx = app->playlist.current_idx;
    app->ui.art_st.initialized = false;
}

void play_at_index(app_instance *app, int index, bool clear)
{
    const fs_entry_t *entry = playlist_play(&app->playlist, index);
    if (entry == NULL)
        return;

    char *file = entry->path.buf;

    audio_source src = audio_from_file(file);
    if (errno != 0)
    {
        play_next(app);
        log_error("Failed to play %s\n", file);
        return;
    }

    attach_static_effect(&src);
    if (clear)
    {
        mixer_clear(&app->audio->mixer);
    }

    array_append(&app->audio->mixer.sources, &src, 1);
    app->ui.playlist_st.hovered_idx = app->playlist.current_idx;
    app->ui.art_st.initialized = false;
}

stream_mute_ctx mute_stream(FILE *stream)
{
    stream_mute_ctx h = {.saved_fd = -1, .stream = stream};
    fflush(stream);
    h.saved_fd = dup_func(fileno(stream));
    if (h.saved_fd < 0)
        return h;
    int devnull = open_dev(NUL_PATH, OPEN_FLAGS);
    if (devnull < 0)
    {
        dup2_func(h.saved_fd, fileno(stream));
        close(h.saved_fd);
        h.saved_fd = -1;
        return h;
    }
    if (dup2_func(devnull, fileno(stream)) < 0)
    {
        dup2_func(h.saved_fd, fileno(stream));
        close(h.saved_fd);
        h.saved_fd = -1;
    }
    close(devnull);
    return h;
}

void restore_stream(stream_mute_ctx h)
{
    if (h.saved_fd < 0)
        return;
    fflush(h.stream);
    dup2_func(h.saved_fd, fileno(h.stream));
    close(h.saved_fd);
}
