#include "fs.h"
#include "logger.h"

#include <errno.h>
#include <string.h>

int fs_iter_init(fs_iterator *iter, const char *dir)
{
    DIR *d = opendir(dir);
    if (d == NULL)
    {
        log_error("Could not open directory: %s: %s\n", dir, strerror(errno));
        return -1;
    }

    iter->d = d;
    iter->dir = dir;
    iter->exhausted = 0;

    return 0;
}

bool fs_iter_next(fs_iterator *iter, fs_entry_t *entry_out)
{
    if (iter->exhausted)
        return false;

    struct dirent *ent = NULL;
    do
    {
        ent = readdir(iter->d);
        if (ent == NULL)
        {
            if (errno != 0)
                log_error("Failed to read the next entry: %s\n",
                          strerror(errno));
            iter->exhausted = 1;
            return false;
        }
    } while (ent &&
             (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0));

    entry_out->path = str_create();
    str_catf(&entry_out->path, "%s/%s", iter->dir, ent->d_name);

    entry_out->name = (strview_t){
        .buf = entry_out->path.buf + strlen(iter->dir) + 1,
        .len = strlen(ent->d_name),
    };

    if (stat(entry_out->path.buf, &entry_out->stat) == -1)
    {
        log_error("Failed get file stat: %s: %s\n", entry_out->path.buf,
                  strerror(errno));
    }

    return true;
}

void fs_iter_free(fs_iterator *iter)
{
    if (iter == NULL)
        return;

    closedir(iter->d);
}

bool fs_is_dir(const fs_entry_t *entry)
{
    return entry->stat.st_mode & S_IFDIR;
}

strview_t fs_name(const fs_entry_t *entry)
{
    char *name = strrchr(entry->path.buf, '/');
    name = name ? name + 1 : entry->path.buf;
    return (strview_t){
        .buf = name,
        .len = (size_t)(name - entry->path.buf),
    };
}
strview_t fs_suffix(const fs_entry_t *entry)
{
    char *suffix = strrchr(entry->path.buf, '.');
    suffix = suffix ? suffix + 1 : entry->path.buf;
    return (strview_t){
        .buf = suffix,
        .len = (size_t)(suffix - entry->path.buf),
    };
}
