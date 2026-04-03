#include "playlist.h"
#include "app.h"
#include "array.h"
#include "audio_source.h"
#include "clock.h"
#include "logger.h"
#include "pathlib.h"
#include "ui.h"
#include <locale.h>
#include <stddef.h>
#include <string.h>

static int wrap_around(int n, int low, int high)
{
    int range = high - low + 1;
    int offset = (n - low) % range;
    if (offset < 0)
        offset += range;
    return low + offset;
}
static void playlist_do_sort(playlist_manager *pl);

void playlist_init(playlist_manager *pl)
{
    setlocale(LC_COLLATE, "");
    pl->files = array_create(16, sizeof(fs_entry_t));
    pl->indices = array_create(16, sizeof(int));
    pl->current_idx = 0;
    pl->current_file = NULL;
    pl->loop = PLAYLIST_LOOP;
    pl->sort = PLAYLIST_SORT_CTIME;
    pl->sort_direction = PLAYLIST_SORT_DESCENDING;
}

void playlist_free(playlist_manager *pl)
{
    fs_entry_t *entry;
    ARR_FOREACH_BYREF(pl->files, entry, i)
    {
        str_free(&entry->path);
    }
    array_free(&pl->files);
    array_free(&pl->indices);
}

void playlist_add(playlist_manager *pl, const char *root)
{
    fs_iterator iter = {0};
    fs_iter_init(&iter, root);

    fs_entry_t entry = {0};
    while (fs_iter_next(&iter, &entry))
    {
        if (fs_is_dir(&entry))
            continue;

        array_append(&pl->files, &entry, 1);

        int idx = pl->files.length - 1;
        array_append(&pl->indices, &idx, 1);
    }
    fs_iter_free(&iter);

    playlist_do_sort(pl);

    log_debug("Loaded %d files from %s\n", pl->files.length, root);
}

void playlist_add_file(playlist_manager *pl, const char *file)
{
    fs_entry_t ent = {0};
    ent.path = str_new(file);
    ent.name = path_name((char *)file);
    array_append(&pl->files, &ent, 1);

    int idx = pl->files.length - 1;
    array_append(&pl->indices, &idx, 1);
}

void playlist_remove(playlist_manager *pl, int index)
{
    if (index < 0 || index >= pl->indices.length)
        return;

    int file_idx = ARR_AS(pl->indices, int)[index];
    fs_entry_t *entry = &ARR_AS(pl->files, fs_entry_t)[file_idx];
    str_free(&entry->path);

    array_remove(&pl->files, file_idx, 1);
    array_remove(&pl->indices, index, 1);

    for (int i = 0; i < pl->indices.length; ++i)
    {
        int *v = &ARR_AS(pl->indices, int)[i];
        if (*v > file_idx)
            (*v)--;
    }
    log_debug("Removed playlist entry at position %d\n", index);
}

static void change_current_file(playlist_manager *pl)
{
    if (pl->current_idx < 0 || pl->current_idx >= pl->indices.length)
    {
        log_error("Playlist index out of bound (%d / %d)\n", pl->current_idx,
                  pl->indices.length);
        return;
    }
    int file_idx = ARR_AS(pl->indices, int)[pl->current_idx];
    pl->current_file = &ARR_AS(pl->files, fs_entry_t)[file_idx];
}

const fs_entry_t *playlist_next(playlist_manager *pl)
{
    if (pl->loop == PLAYLIST_LOOP_TRACK)
        return pl->current_file;

    if (pl->loop == PLAYLIST_NO_LOOP &&
        pl->current_idx + 1 > pl->indices.length - 1)
        return NULL;

    pl->current_idx =
        wrap_around(pl->current_idx + 1, 0, pl->indices.length - 1);

    change_current_file(pl);
    log_debug("Next file %s\n", pl->current_file->path.buf);
    return pl->current_file;
}

const fs_entry_t *playlist_prev(playlist_manager *pl)
{
    if (pl->loop == PLAYLIST_LOOP_TRACK)
        return pl->current_file;

    if (pl->loop == PLAYLIST_NO_LOOP && pl->current_idx - 1 < 0)
        return NULL;

    pl->current_idx =
        wrap_around(pl->current_idx - 1, 0, pl->indices.length - 1);

    change_current_file(pl);
    log_debug("Prev file %s\n", pl->current_file->path.buf);
    return pl->current_file;
}

const fs_entry_t *playlist_play(playlist_manager *pl, int index)
{
    if (index < 0 || index >= pl->indices.length)
        return pl->current_file;
    pl->current_idx = index;
    change_current_file(pl);
    log_debug("Play file %s (playlist pos %d)\n", pl->current_file->path.buf,
              index);
    return pl->current_file;
}

fs_entry_t *playlist_get_at_index(playlist_manager *pl, int index)
{
    if (index < 0 || index > pl->indices.length - 1)
    {
        log_error("Playlist index out of bound (%d / %d)\n", pl->current_idx,
                  pl->indices.length);
        return NULL;
    }

    int i = ARR_AS(pl->indices, int)[index];
    return &ARR_AS(pl->files, fs_entry_t)[i];
}

static int find_file_index(playlist_manager *pl, const fs_entry_t *ent)
{
    int file_idx = ent - ARR_AS(pl->files, fs_entry_t);
    int index;
    ARR_FOREACH(pl->indices, index, i)
    {
        if (index == file_idx)
            return i;
    }

    return -1;
}

static void playlist_do_sort(playlist_manager *pl)
{
    int n = pl->files.length;
    if (n == 0)
    {
        pl->current_idx = -1;
        pl->current_file = NULL;
        return;
    }

    int *inds = ARR_AS(pl->indices, int);
    fs_entry_t *files = ARR_AS(pl->files, fs_entry_t);
    const fs_entry_t *prev = pl->current_file;

    for (int i = 0; i < n - 1; ++i)
    {
        for (int j = i + 1; j < n; ++j)
        {
            fs_entry_t *a = &files[inds[i]];
            fs_entry_t *b = &files[inds[j]];
            int cmp = 0;
            switch (pl->sort)
            {
            case PLAYLIST_SORT_CTIME:
                cmp = (a->stat.st_ctime < b->stat.st_ctime)   ? -1
                      : (a->stat.st_ctime > b->stat.st_ctime) ? 1
                                                              : 0;
                break;
            case PLAYLIST_SORT_MTIME:
                cmp = (a->stat.st_mtime < b->stat.st_mtime)   ? -1
                      : (a->stat.st_mtime > b->stat.st_mtime) ? 1
                                                              : 0;
                break;
            case PLAYLIST_SORT_NAME:
                cmp = strcoll(a->path.buf, b->path.buf);
                break;
            default:
                break;
            }
            if (pl->sort_direction == PLAYLIST_SORT_DESCENDING)
                cmp = -cmp;
            if (cmp > 0)
            {
                int tmp = inds[i];
                inds[i] = inds[j];
                inds[j] = tmp;
            }
        }
    }

    pl->current_idx = find_file_index(pl, prev);
}

static void playlist_reverse(playlist_manager *pl)
{
    const fs_entry_t *prev = pl->current_file;
    array_reverse(&pl->indices);
    pl->current_idx = find_file_index(pl, prev);
}

void playlist_sort(playlist_manager *pl, enum playlist_sort method,
                   enum playlist_sort_direction sort_direction)
{
    if (method == pl->sort && sort_direction != pl->sort_direction)
    {
        pl->sort_direction = sort_direction;
        playlist_reverse(pl);
        return;
    }

    pl->sort = method;
    pl->sort_direction = sort_direction;
    playlist_do_sort(pl);
    pl->is_shuffled = false;
}

void playlist_shuffle(playlist_manager *pl)
{
    const fs_entry_t *prev = pl->current_file;
    array_shuffle(&pl->indices);
    pl->current_idx = find_file_index(pl, prev);
    pl->is_shuffled = true;
}

cJSON *playlist_serialize(playlist_manager *pl)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        goto err;

    {
        cJSON *info = cJSON_CreateObject();
        if (info == NULL)
            goto err;
        cJSON *loop = cJSON_CreateNumber((int)pl->loop);
        if (loop == NULL)
            goto err;
        if (!cJSON_AddItemToObject(info, "loop", loop))
            goto err;

        cJSON *sort = cJSON_CreateNumber((int)pl->sort);
        if (sort == NULL)
            goto err;
        if (!cJSON_AddItemToObject(info, "sort", sort))
            goto err;

        cJSON *sort_direction = cJSON_CreateNumber((int)pl->sort_direction);
        if (sort_direction == NULL)
            goto err;
        if (!cJSON_AddItemToObject(info, "sort_direction", sort_direction))
            goto err;

        cJSON *current_idx = cJSON_CreateNumber(pl->current_idx);
        if (current_idx == NULL)
            goto err;
        if (!cJSON_AddItemToObject(info, "current_idx", current_idx))
            goto err;

        // TODO: figure out a way to handle multiple sources here
        app_instance *app = app_get();
        if (app->audio->mixer.sources.length > 0)
        {
            audio_source *src =
                &ARR_AS(app->audio->mixer.sources, audio_source)[0];
            if (!src->is_realtime)
            {
                cJSON *current_playhead =
                    cJSON_CreateNumber((int64_t)US2MS(src->timestamp));
                if (current_playhead == NULL)
                    goto err;
                if (!cJSON_AddItemToObject(info, "current_playhead",
                                           current_playhead))
                    goto err;
            }
        }

        cJSON *is_shuffled = cJSON_CreateBool(pl->is_shuffled);
        if (is_shuffled == NULL)
            goto err;
        if (!cJSON_AddItemToObject(info, "is_shuffled", is_shuffled))
            goto err;
        if (!cJSON_AddItemToObject(root, "info", info))
            goto err;
    }

    {
        cJSON *files = cJSON_CreateArray();
        if (files == NULL)
            goto err;

        fs_entry_t *file;
        ARR_FOREACH_BYREF(pl->files, file, _)
        {
            cJSON *f = cJSON_CreateString(file->path.buf);
            if (f == NULL)
                goto err;

            if (!cJSON_AddItemToArray(files, f))
                goto err;
        }
        if (!cJSON_AddItemToObject(root, "files", files))
            goto err;
    }

    {
        cJSON *indices = cJSON_CreateArray();
        if (indices == NULL)
            goto err;
        int idx;
        ARR_FOREACH(pl->indices, idx, _)
        {
            cJSON *i = cJSON_CreateNumber(idx);
            if (i == NULL)
                goto err;

            if (!cJSON_AddItemToArray(indices, i))
                goto err;
        }
        if (!cJSON_AddItemToObject(root, "indices", indices))
            goto err;
    }

    return root;

err:
    if (root)
        cJSON_Delete(root);
    return NULL;
}

int playlist_deserialize(playlist_manager *pl, cJSON *root)
{
    // TODO: default value
    {
        cJSON *info = cJSON_GetObjectItem(root, "info");
        if (info == NULL || !cJSON_IsObject(info))
            goto err;

        cJSON *loop = cJSON_GetObjectItem(info, "loop");
        if (loop == NULL || !cJSON_IsNumber(loop))
            goto err;
        pl->loop = cJSON_GetNumberValue(loop);

        cJSON *sort = cJSON_GetObjectItem(info, "sort");
        if (sort == NULL || !cJSON_IsNumber(sort))
            goto err;
        pl->sort = cJSON_GetNumberValue(sort);

        cJSON *sort_direction = cJSON_GetObjectItem(info, "sort_direction");
        if (sort_direction == NULL || !cJSON_IsNumber(sort_direction))
            goto err;
        pl->sort_direction = cJSON_GetNumberValue(sort_direction);

        cJSON *current_idx = cJSON_GetObjectItem(info, "current_idx");
        if (current_idx == NULL || !cJSON_IsNumber(current_idx))
            goto err;
        pl->current_idx = cJSON_GetNumberValue(current_idx);

        cJSON *current_playhead = cJSON_GetObjectItem(info, "current_playhead");
        if (current_playhead == NULL)
            goto err;
        app_instance *app = app_get();
        app->want_to_seek_ms = (int64_t)cJSON_GetNumberValue(current_playhead);

        cJSON *is_shuffled = cJSON_GetObjectItem(info, "is_shuffled");
        if (is_shuffled == NULL || !cJSON_IsBool(is_shuffled))
            goto err;
        pl->is_shuffled = cJSON_IsTrue(is_shuffled);
    }

    {
        cJSON *files = cJSON_GetObjectItem(root, "files");
        if (files == NULL || !cJSON_IsArray(files))
            goto err;
        cJSON *file;
        cJSON_ArrayForEach(file, files)
        {
            char *s = cJSON_GetStringValue(file);
            if (s == NULL)
                goto err;

            fs_entry_t ent = {0};
            ent.path = str_new(s);
            ent.name = path_name(ent.path.buf);
            array_append(&pl->files, &ent, 1);
        }
    }

    {
        cJSON *indices = cJSON_GetObjectItem(root, "indices");
        if (indices == NULL || !cJSON_IsArray(indices))
            goto err;
        cJSON *idx;
        cJSON_ArrayForEach(idx, indices)
        {
            int i = cJSON_GetNumberValue(idx);
            array_append(&pl->indices, &(int){i}, 1);
        }
    }
    return 0;
err:
    return -1;
}
