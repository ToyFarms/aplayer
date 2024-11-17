#ifndef __FS_H
#define __FS_H

#include <stdbool.h>
#include <stdio.h>

#ifdef _WIN32
#  error PATHMAX not defined
#  define PATHMAX 0
#elif defined(__linux__)
#  include <linux/limits.h>
#  define PATHMAX (PATH_MAX + 255)
#endif // _WIN32

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

int fs_normpath(const char *path, char *output, size_t max);
int fs_cmppath(const char *a, const char *b);

#endif /* __FS_H */
