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

#endif /* __FS_H */
