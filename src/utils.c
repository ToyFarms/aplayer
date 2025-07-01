#include "utils.h"
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
    str_t s = str_alloc(strlen(str));

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

    printf("%s", s.buf);
    str_free(&s);
}

void play_next(app_instance *app)
{
    const fs_entry_t *entry = playlist_next(&app->playlist);
    if (entry == NULL)
        return;

    char *file = entry->path.buf;
    audio_source src =
        audio_from_file(file, app->audio->nb_channels, app->audio->sample_rate,
                        app->audio->sample_fmt);
    if (errno != 0)
    {
        play_next(app);
        log_error("Failed to play %s\n", file);
        return;
    }

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
    audio_source src =
        audio_from_file(file, app->audio->nb_channels, app->audio->sample_rate,
                        app->audio->sample_fmt);
    if (errno != 0)
    {
        play_prev(app);
        log_error("Failed to play %s\n", file);
        return;
    }

    mixer_clear(&app->audio->mixer);
    array_append(&app->audio->mixer.sources, &src, 1);
    app->ui.playlist_st.hovered_idx = app->playlist.current_idx;
    app->ui.art_st.initialized = false;
}

void play_at_index(app_instance *app, int index)
{
    const fs_entry_t *entry = playlist_play(&app->playlist, index);
    if (entry == NULL)
        return;

    char *file = entry->path.buf;
    audio_source src =
        audio_from_file(file, app->audio->nb_channels, app->audio->sample_rate,
                        app->audio->sample_fmt);
    if (errno != 0)
    {
        play_next(app);
        log_error("Failed to play %s\n", file);
        return;
    }

    mixer_clear(&app->audio->mixer);
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
