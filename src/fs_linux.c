#include "dict.h"
#include "fs.h"
#include "logger.h"
#include "queue.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

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

// TODO: complete this

// typedef struct fsmon_watch_t
// {
//     int wd;
//     char *path;
//     int flags;
//     int active;
// } fsmon_watch_t;
//
// typedef struct fsmon_t
// {
//     int inotify_fd;
//     dict_t /* of fsmon_watch_t */ watches;
//     queue_t /* of fsmon_event */ events;
// } fsmon_t;
//
// fsmon_t *fsmon_create()
// {
//     fsmon_t *mon = calloc(1, sizeof(*mon));
//     if (mon == NULL)
//     {
//         log_error("Failed to allocate monitor object\n");
//         goto error;
//     }
//
//     mon->inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
//     if (mon->inotify_fd == -1)
//     {
//         log_error("Failed to initialize inotify: %s\n", strerror(errno));
//         goto error;
//     }
//
//     mon->watches = dict_create();
//     if (errno != 0)
//     {
//         log_error("Failed to create dict for watches: %s\n", strerror(errno));
//         goto error;
//     }
//
//     mon->events = queue_create();
//     if (errno != 0)
//     {
//         log_error("Failed to create queue for fsmon event: %s\n",
//                   strerror(errno));
//         goto error;
//     }
//
//     return mon;
// error:
//     if (mon)
//     {
//         free(mon);
//         if (mon->watches.bucket_slot > 0)
//             dict_free(&mon->watches);
//         queue_free(&mon->events);
//         close(mon->inotify_fd);
//     }
//
//     return NULL;
// }
//
// void fsmon_free(fsmon_t *mon)
// {
//     if (mon == NULL)
//         return;
//
//     fsmon_watch_t *watch;
//     DICT_FOREACH(key, watch, i, &mon->watches)
//     {
//         inotify_rm_watch(mon->inotify_fd, watch->wd);
//         free(watch->path);
//     }
//     DICT_FOREACH_END;
//
//     dict_free(&mon->watches);
//     queue_free(&mon->events);
//     close(mon->inotify_fd);
//
//     free(mon);
// }
//
// int fsmon_watch(fsmon_t *mon, const char *path, int flags)
// {
//     if (dict_exists(&mon->watches, path))
//         return FSMON_ALREADY_WATCHING;
//
//     return FSMON_OK;
// }
//
// const fsmon_event *fsmon_poll(fsmon_t *mon);
