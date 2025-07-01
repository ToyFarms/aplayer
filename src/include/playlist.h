#ifndef __PLAYLIST_H
#define __PLAYLIST_H

#include "array.h"
#include "fs.h"

enum playlist_loop
{
    PLAYLIST_LOOP,
    PLAYLIST_LOOP_TRACK,
    PLAYLIST_NO_LOOP,
};

enum playlist_sort
{
    PLAYLIST_SORT_CTIME,
    PLAYLIST_SORT_MTIME,
    PLAYLIST_SORT_NAME,
    PLAYLIST_SORT_LENGTH,
};

static inline const char *playlist_sort_name(enum playlist_sort sort)
{
    switch (sort)
    {
    case PLAYLIST_SORT_CTIME:
        return "ctime";
    case PLAYLIST_SORT_MTIME:
        return "mtime";
    case PLAYLIST_SORT_NAME:
        return "name";
    default:
        return "sort_unknown";
    }
}

enum playlist_sort_direction
{
    PLAYLIST_SORT_ASCENDING,
    PLAYLIST_SORT_DESCENDING,
};

static inline const char *playlist_sort_dir_name(
    enum playlist_sort_direction dir)
{
    switch (dir)
    {
    case PLAYLIST_SORT_ASCENDING:
        return "asc";
    case PLAYLIST_SORT_DESCENDING:
        return "desc";
    default:
        return "sort_dir_unknown";
    }
}

typedef struct playlist_manager
{
    array(fs_entry_t) files;
    array(int) indices;
    enum playlist_loop loop;
    enum playlist_sort sort;
    enum playlist_sort_direction sort_direction;

    int current_idx;
    const fs_entry_t *current_file;
    bool is_shuffled;
} playlist_manager;

void playlist_init(playlist_manager *pl);
void playlist_free(playlist_manager *pl);
void playlist_add(playlist_manager *pl, const char *root);
const fs_entry_t *playlist_next(playlist_manager *pl);
const fs_entry_t *playlist_prev(playlist_manager *pl);
const fs_entry_t *playlist_play(playlist_manager *pl, int index);
fs_entry_t *playlist_get_at_index(playlist_manager *pl, int index);
void playlist_remove(playlist_manager *pl, int index);
void playlist_sort(playlist_manager *pl, enum playlist_sort method,
                   enum playlist_sort_direction sort_direction);
void playlist_shuffle(playlist_manager *pl);

#endif /* __PLAYLIST_H */
