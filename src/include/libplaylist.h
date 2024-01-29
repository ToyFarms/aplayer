#ifndef _LIBPLAYLIST_H
#define _LIBPLAYLIST_H

#include <libavutil/log.h>

#include "libfile.h"
#include "libplayer.h"
#include "libaudio.h"
#include "libhelper.h"

typedef struct Playlist
{
    File *entries;
    int entry_size;
    int playing_idx;

    PlayerState *pst;
} Playlist;

Playlist *playlist_init(char *directory, PlayerState *pst, bool init_entry);
void playlist_free();
void playlist_refresh_entry(char *directory);
void playlist_update_entry(File *entries, int entry_size);
void playlist_next(void(*finished_callback)(void));
void playlist_prev(void(*finished_callback)(void));
void playlist_play(void(*finished_callback)(void));
void playlist_play_idx(int index, void(*finished_callback)(void));

#endif /* _LIBPLAYLIST_H */ 