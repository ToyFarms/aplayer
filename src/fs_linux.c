#include "fs.h"
#include "logger.h"

#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>

fs_root fs_readdir(const char *_path, int flags)
{
    const char *path = _path;
#ifdef _WIN32
#  error wordexp() alt not implemented
#else
    wordexp_t exp;
    wordexp(_path, &exp, 0);

    path = exp.we_wordv[0];
#endif // _WIN32

    int initcap = 32;
    fs_root root;
    root.entries = calloc(sizeof(*root.entries), initcap);
    root.len = 0;
    root.capacity = initcap;
    root.base = strdup(path);
    root.baselen = strlen(path);
    DIR *dir = NULL;

    if (path == NULL)
    {
        log_error("path is NULL\n");
        goto exit;
    }

    dir = opendir(path);
    if (dir == NULL)
    {
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
    if (dir != NULL)
        closedir(dir);
#ifdef _WIN32
#  error wordexp() alt not implemented
#else
    wordfree(&exp);
#endif // _WIN32
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
