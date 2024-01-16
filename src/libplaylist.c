#include "libplaylist.h"

#define PLAYLIST_CHECK_INITIALIZED(fn_name, ret)                                                                  \
    do                                                                                                            \
    {                                                                                                             \
        if (!pl)                                                                                                  \
        {                                                                                                         \
            av_log(NULL, AV_LOG_WARNING, "%s %s:%d Playlist is not initialized.\n", fn_name, __FILE__, __LINE__); \
            ret;                                                                                                  \
        }                                                                                                         \
    } while (0)

static Playlist *pl;

Playlist *playlist_init(char *directory, PlayerState *pst)
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing Playlist.\n");
    pl = (Playlist *)malloc(sizeof(Playlist));

    if (!pl)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate Playlist.\n");
        return NULL;
    }

    memset(pl, 0, sizeof(pl));

    pl->entries = list_directory(directory, &pl->entry_size);
    pl->playing_idx = -1;
    pl->pst = pst;

    return pl;
}

void playlist_free()
{
    PLAYLIST_CHECK_INITIALIZED("playlist_free", return);

    av_log(NULL, AV_LOG_DEBUG, "Free Playlist.\n");

    if (pl->entries)
    {
        av_log(NULL, AV_LOG_DEBUG, "Free Playlist entries.\n");
        for (int i = 0; i < pl->playing_idx; i++)
        {
            file_free(pl->entries[i]);
        }

        free(pl->entries);
    }

    free(pl);
    pl = NULL;
}

void playlist_next(void(*finished_callback)(void))
{
    PLAYLIST_CHECK_INITIALIZED("playlist_next", return);

    av_log(NULL, AV_LOG_DEBUG, "Playlist next.\n");

    pl->playing_idx = wrap_around(pl->playing_idx + 1, 0, pl->entry_size);
    playlist_play(finished_callback);
}

void playlist_prev(void(*finished_callback)(void))
{
    PLAYLIST_CHECK_INITIALIZED("playlist_prev", return);

    av_log(NULL, AV_LOG_DEBUG, "Playlist prev.\n");

    pl->playing_idx = wrap_around(pl->playing_idx - 1, 0, pl->entry_size);
    playlist_play(finished_callback);
}

void playlist_play(void (*finished_callback)(void))
{
    PLAYLIST_CHECK_INITIALIZED("playlist_play", return);

    audio_play();

    if (_audio_get_stream())
    {
        audio_exit();
        audio_wait_until_finished();
    }

    audio_start_async(pl->entries[pl->playing_idx].path, finished_callback);
    audio_wait_until_initialized();
}

void playlist_play_idx(int index, void (*finished_callback)(void))
{
    PLAYLIST_CHECK_INITIALIZED("playlist_play_idx", return);

    pl->playing_idx = index;
    playlist_play(finished_callback);
}