#ifndef __FS_H
#define __FS_H

#include <stdbool.h>

#define FS_FILTER_DIR  (1 << 0)
#define FS_FILTER_FILE (1 << 1)

typedef struct entry_t
{
    char name[256];
    int namelen;
    bool isdir;
} entry_t;

typedef struct fs_root
{
    char *base;
    int baselen;

    entry_t *entries;
    int len;
    int capacity;
} fs_root;

fs_root fs_readdir(const char *path, int flags);
void fs_root_free(fs_root *root);

#endif /* __FS_H */
