#include "fs.h"
#include "logger.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wordexp.h>

fs_root fs_readdir(const char *_path, int flags)
{
    errno = 0;
    const char *path = _path;

    wordexp_t exp;
    wordexp(_path, &exp, 0);

    path = exp.we_wordv[0];

    int initcap = 32;
    fs_root root = {0};
    root.entries = calloc(sizeof(*root.entries), initcap);
    root.len = 0;
    root.capacity = initcap;
    root.base = strdup(path);
    root.baselen = strlen(path);
    DIR *dir = NULL;
    int errnb = 0;

    if (path == NULL)
    {
        errnb = errno;
        log_error("path is NULL\n");
        goto exit;
    }

    dir = opendir(path);
    if (dir == NULL)
    {
        errnb = errno;
        log_error("Failed to open path '%s'\n", path);
        goto exit;
    }

    struct dirent *file = NULL;

    while ((file = readdir(dir)) != NULL)
    {
        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0)
            continue;

        bool isdir = file->d_type == DT_DIR;
        if (flags & FS_FILTER_DIR && isdir)
            continue;
        else if (flags & FS_FILTER_FILE && !isdir)
            continue;

        if (root.len >= root.capacity)
        {
            root.capacity *= 2;
            root.entries =
                realloc(root.entries, root.capacity * sizeof(*root.entries));
        }

        entry_t entry;
        memcpy(entry.name, file->d_name, NAME_MAX);
        size_t namelen = strlen(file->d_name);
        entry.name[namelen] = '\0';
        entry.namelen = namelen;
        entry.isdir = isdir;

        root.entries[root.len++] = entry;
    }

exit:
    errno = errnb;
    if (dir != NULL)
        closedir(dir);

    wordfree(&exp);

    return root;
}

void fs_root_free(fs_root *root)
{
    if (root == NULL)
        return;

    free(root->base);
    root->base = NULL;

    free(root->entries);
    root->entries = NULL;

    memset(root, 0, sizeof(*root));
}

int fs_normpath(const char *path, char *output, size_t max)
{
    wordexp_t exp;
    wordexp(path, &exp, 0);

    strncpy(output, exp.we_wordv[0], max);
    int len = strlen(exp.we_wordv[0]);

    wordfree(&exp);

    return len;
}

int fs_cmppath(const char *a, const char *b)
{
    struct stat stat1, stat2;

    if (stat(a, &stat1) == -1)
    {
        log_error("Failed getting stat() for '%s'", a);
        return -1;
    }

    if (stat(b, &stat2) == -1)
    {
        log_error("Failed getting stat() for '%s'", b);
        return -1;
    }

    return stat1.st_ino == stat2.st_ino && stat1.st_dev == stat2.st_dev;
}
