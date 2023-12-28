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

Playlist *playlist_init(char *directory, PlayerState *pst);
void playlist_free();
void playlist_next();
void playlist_prev();
void playlist_play();
void playlist_play_idx(int index);

#endif /* _LIBPLAYLIST_H */ 