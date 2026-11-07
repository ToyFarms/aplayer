#include "dict.h"
#include "fs.h"
#include "logger.h"
#include "queue.h"
#include <errno.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

int fs_iter_init(fs_iterator *iter, const char *dir)
{
    memset(iter, 0, sizeof(*iter));

    str_t pattern = str_create();
    str_catf(&pattern, "%s\\*", dir);

    WIN32_FIND_DATAW find_data;
    HANDLE h = FindFirstFileW(str_as_wide(&pattern), &find_data);

    str_free(&pattern);

    if (h == INVALID_HANDLE_VALUE)
    {
        log_error("Could not open directory: %s: %lu\n", dir, GetLastError());
        return -1;
    }

    iter->h = h;
    iter->dir = dir;
    iter->dirw = str_decode(
        &(str_t){.buf = (char *)dir,
                 .len = strlen(dir)});
    iter->find_data = find_data;
    iter->has_pending = 1;
    iter->exhausted = 0;

    return 0;
}

bool fs_iter_next(fs_iterator *iter, fs_entry_t *entry_out)
{
    if (iter->exhausted)
        return false;

    for (;;)
    {
        if (!iter->has_pending)
        {
            if (!FindNextFileW(iter->h, &iter->find_data))
            {
                DWORD err = GetLastError();
                if (err != ERROR_NO_MORE_FILES)
                    log_error("Failed to read the next entry: %lu\n", err);
                iter->exhausted = 1;
                return false;
            }
        }
        iter->has_pending = 0;

        const wchar_t *namew = iter->find_data.cFileName;
        if (wcscmp(namew, L".") == 0 || wcscmp(namew, L"..") == 0)
            continue;

        str_t name = str_encode(namew);
        entry_out->path = str_create();
        str_catf(&entry_out->path, "%s/%s", iter->dir, name.buf);
        entry_out->name = (strview_t){
            .buf = entry_out->path.buf + strlen(iter->dir) + 1,
            .len = name.len,
        };
        str_free(&name);

        wchar_t *fullpath = str_as_wide(&entry_out->path);
        if (_wstat(fullpath, &entry_out->stat) == -1)
        {
            log_error("Failed to stat: %ls: %s\n", fullpath, strerror(errno));
        }

        return true;
    }
}

void fs_iter_free(fs_iterator *iter)
{
    if (iter == NULL)
        return;

    if (iter->h != INVALID_HANDLE_VALUE)
        FindClose(iter->h);

    free(iter->dirw);
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
        .len = strlen(name),
    };
}

strview_t fs_suffix(const fs_entry_t *entry)
{
    char *suffix = strrchr(entry->path.buf, '.');
    suffix = suffix ? suffix + 1 : entry->path.buf;
    return (strview_t){
        .buf = suffix,
        .len = strlen(suffix),
    };
}