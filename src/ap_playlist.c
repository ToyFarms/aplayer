#include "ap_playlist.h"

APPlaylist *ap_playlist_alloc()
{
    APPlaylist *p = calloc(1, sizeof(*p));
    if (!p)
        return NULL;

    ap_playlist_init(p);

    return p;
}

void ap_playlist_init(APPlaylist *p)
{
    assert(p != NULL);
    p->groups = ap_array_alloc(16, sizeof(APEntryGroup));
    p->sources = ap_array_alloc(16, sizeof(APSource));
}

void ap_playlist_free(APPlaylist *p)
{
    if (!p)
        return;

    for (int i = 0; i < p->groups->len; i++)
    {
        APEntryGroup group = ARR_INDEX(p->groups, APEntryGroup *, i);
        free(group.id);
        group.id = NULL;
        for (int j = 0; j < group.entry->len; j++)
        {
            APFile file = ARR_INDEX(group.entry, APFile *, j);
            free(file.filename);
            file.filename = NULL;
            free(file.directory);
            file.directory = NULL;
        }
        ap_array_free(&group.entry);
    }
    ap_array_free(&p->groups);
    ap_array_free(&p->sources);

    free(p);
}

void ap_playlist_deserialize(APPlaylist *p, const char *bytes);
char *ap_playlist_serialize(APPlaylist *p);

void ap_playlist_load(APPlaylist *p)
{
    if (!p->groups)
        ap_array_init(p->groups, 128, sizeof(APEntryGroup));
    T(APFile *) APArray *root = ap_array_alloc(128, sizeof(APFile));
    for (int i = 0; i < p->sources->len; i++)
    {
        APSource src = ARR_INDEX(p->sources, APSource *, i);
        if (src.is_file)
            ap_array_append_resize(root,
                                   &(APFile){"*root*", strdup(src.path),
                                             file_get_stat(src.path, NULL)},
                                   1);
        else
        {
            T(APFile *) APArray *files = read_directory(src.path);
            // TODO: Report error
            if (!files)
                continue;
            ap_array_append_resize(p->groups,
                                   &(APEntryGroup){strdup(src.path), files}, 1);
        }
    }

    ap_array_append_resize(p->groups, &(APEntryGroup){"*root*", root}, 1);
}
