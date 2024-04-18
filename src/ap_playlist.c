#include "ap_playlist.h"

void ap_playlist_deserialize(APPlaylist *p, const char *bytes);
char *ap_playlist_serialize(APPlaylist *p);

void ap_playlist_load(APPlaylist *p)
{
    ap_array_init(p->entries, 128, sizeof(APEntryGroup));
    T(APFile *) APArray *root = ap_array_alloc(128, sizeof(APFile));
    for (int i = 0; i < p->sources->len; i++)
    {
        APSource src = ARR_INDEX(p->sources, APSource *, i);
        if (src.is_file)
            ap_array_append_resize(
                root,
                &(APFile){"*root*", src.path, file_get_stat(src.path, NULL)},
                1);
        else
        {
            T(APFile *) APArray *files = read_directory(src.path);
            // TODO: Report error
            if (!files)
                continue;
            ap_array_append_resize(p->entries, &(APEntryGroup){src.path, files},
                                   1);
        }
    }

    ap_array_append_resize(p->entries, &(APEntryGroup){"*root*", root}, 1);
}