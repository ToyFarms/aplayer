#ifndef __AP_PLAYLIST_H
#define __AP_PLAYLIST_H

#include "ap_array.h"
#include "ap_utils.h"
#include "sds.h"
#include <stdbool.h>

#define PLAYLIST_VERSION_MAJOR 1
#define PLAYLIST_VERSION_MINOR 0
#define PLAYLIST_VERSION_PATCH 0

#define PLAYLIST_FLAG_FILE (1 << 0)
#define PLAYLIST_FLAG_EXPAND (1 << 1)

typedef struct APSource
{
    char *path;
    bool is_file;

    /* specify whether to serialize all the files or only the directory */
    bool expand;
} APSource;

typedef struct APEntryGroup
{
    char *id;
    T(APFile) APArray *entries;
} APEntryGroup;

typedef struct APPlaylist
{
    T(APSource) APArray *sources;
    T(APEntryGroup) APArray *groups;
} APPlaylist;

APPlaylist *ap_playlist_alloc();
void ap_playlist_init(APPlaylist *p);
void ap_playlist_free_member(APPlaylist *p);
void ap_playlist_free(APPlaylist **p);
void ap_playlist_deserialize(APPlaylist *p, void *buf, int buf_size);
/* if expand_source is true, it will also serialize the file list, not just the
 * source */
void *ap_playlist_serialize(APPlaylist *p, bool expand_source, int *out_size);
/* load entries from sources */
void ap_playlist_load(APPlaylist *p);

#endif /* __AP_PLAYLIST_H */
