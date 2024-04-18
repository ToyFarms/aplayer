#ifndef __AP_PLAYLIST_H
#define __AP_PLAYLIST_H

#include "ap_array.h"
#include <stdbool.h>
#include "ap_utils.h"

typedef struct APSource
{
    const char *path;
    bool is_file;
} APSource;

typedef struct APEntryGroup
{
    const char *id;
    T(APFile *) APArray *entry;
} APEntryGroup;

typedef struct APPlaylist
{
    T(APSource) APArray *sources;
    T(APEntryGroup) APArray *entries;
} APPlaylist;

void ap_playlist_deserialize(APPlaylist *p, const char *bytes);
char *ap_playlist_serialize(APPlaylist *p);
/* load entries from sources */
void ap_playlist_load(APPlaylist *p);

#endif /* __AP_PLAYLIST_H */
