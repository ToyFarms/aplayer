#ifndef __FS_H
#define __FS_H

#include "ds.h"

#include <stdbool.h>
#include <sys/stat.h>
#ifdef _WIN32
#  error "NOT IMPLEMENTED"
#else
#  include <dirent.h>
#endif // _WIN32

typedef struct fs_entry_t
{
    struct stat stat;
    str_t path;
    strview_t name;
} fs_entry_t;

typedef struct fs_iterator
{
    const char *dir;
#ifdef _WIN32
#  error "NOT IMPLEMENTED"
#else
    DIR *d;
#endif // _WIN32
    int exhausted;
} fs_iterator;

int fs_iter_init(fs_iterator *iter, const char *dir);
bool fs_iter_next(fs_iterator *iter, fs_entry_t *entry_out);
void fs_iter_free(fs_iterator *iter);
bool fs_is_dir(const fs_entry_t *entry);
strview_t fs_name(const fs_entry_t *entry);
strview_t fs_suffix(const fs_entry_t *entry);

typedef struct fsmon_t fsmon_t;

enum fsmon_event_type
{
    DIR_EVENT_UNKNOWN,
    DIR_EVENT_CREATED,
    DIR_EVENT_MODIFIED,
    DIR_EVENT_DELETED,
    DIR_EVENT_MOVED,
};

enum fsmon_error
{
    FSMON_OK,
    FSMON_ALREADY_WATCHING,
};

typedef struct fsmon_event
{
    enum fsmon_event_type type;
    const char *path;
    const char *old_path;
} fsmon_event;

#define FSMON_RECURSIVE (1 << 0)
#define FSMON_FILE_ONLY (1 << 1)
#define FSMON_DIR_ONLY  (1 << 2)

// TODO: add callback api, figure out how to handle poll vs callback

fsmon_t *fsmon_create();
void fsmon_free(fsmon_t *mon);
int fsmon_watch(fsmon_t *mon, const char *path, int flags);
const fsmon_event *fsmon_poll(fsmon_t *mon);

#endif /* __FS_H */
