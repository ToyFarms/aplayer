#include "ap_playlist.h"
#include "ap_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct APDynBuf
{
    void *data;
    int offset;
    int size;
} APDynBuf;

static void ap_dynbuf_init(APDynBuf *buf, int size)
{
    buf->data = calloc(size, 1);
    buf->offset = 0;
    buf->size = size;
}

static void ap_dynbuf_ensure_free(APDynBuf *buf, int size)
{
    if (buf->size - buf->offset >= size)
        return;
    while (buf->offset + size >= buf->size)
        buf->size *= 2;
    buf->data = realloc(buf->data, buf->size);
}

static void ap_playlist_entries_free(APArrayT(APFile) * entries);

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

static void ap_playlist_entries_free(APArrayT(APFile) * entries)
{
    for (int i = 0; i < entries->len; i++)
    {
        APFile file = ARR_INDEX(entries, APFile *, i);
        if (i == 0)
        {
            free(file.directory);
            file.directory = NULL;
        }
        free(file.filename);
        file.filename = NULL;
        free(file.filenamew);
        file.filenamew = NULL;
    }
}

void ap_playlist_free_member(APPlaylist *p)
{
    if (!p)
        return;

    for (int i = 0; i < p->groups->len; i++)
    {
        APEntryGroup group = ARR_INDEX(p->groups, APEntryGroup *, i);
        ap_playlist_entries_free(group.entries);
        ap_array_free(&group.entries);
    }

    ap_array_free(&p->groups);
    ap_array_free(&p->sources);
}

void ap_playlist_free(APPlaylist **p)
{
    PTR_PTR_CHECK(p, );

    ap_playlist_free_member(*p);
    free(*p);
    *p = NULL;
}

#define GET_OFFSET(small_endian, size, i)                                      \
    ((small_endian) ? (i) * 8 : ((size) - (i) - 1) * 8)

static void i64tob(char *buf, int64_t n, bool small_endian)
{
    for (int i = 0; i < 8; i++)
        buf[i] = (n >> GET_OFFSET(small_endian, 64, i)) & 0xFF;
}

static void i32tob(char *buf, int32_t n, bool small_endian)
{
    for (int i = 0; i < 4; i++)
        buf[i] = (n >> GET_OFFSET(small_endian, 32, i)) & 0xFF;
}

static void i16tob(char *buf, int16_t n, bool small_endian)
{
    for (int i = 0; i < 2; i++)
        buf[i] = (n >> GET_OFFSET(small_endian, 16, i)) & 0xFF;
}

static void i8tob(char *buf, int8_t n, bool small_endian)
{
    buf[0] = (n >> GET_OFFSET(small_endian, 8, 0)) & 0xFF;
}

#define BUF_READ(buf, T) (*(T *)((buf).data + (buf).offset))
#define BUF_OFFSET(buf)  ((buf).data + (buf).offset)

void ap_playlist_deserialize(APPlaylist *p, void *_buf, int buf_size)
{
    APDynBuf buf = {_buf, 0, buf_size};
    uint16_t major = BUF_READ(buf, uint16_t);
    buf.offset += 2;
    uint16_t minor = BUF_READ(buf, uint16_t);
    buf.offset += 2;
    uint32_t patch = BUF_READ(buf, uint32_t);
    buf.offset += 4;

    APArrayT(APFile) *root = ap_array_alloc(16, sizeof(APFile));

    uint32_t source_len = BUF_READ(buf, uint32_t);
    buf.offset += 4;
    for (int i = 0; i < source_len; i++)
    {
        uint64_t size = BUF_READ(buf, uint64_t);
        buf.offset += 8;
        int64_t flag = BUF_READ(buf, int64_t);
        buf.offset += 8;
        bool is_file = flag & PLAYLIST_FLAG_FILE;
        bool need_expand = flag & PLAYLIST_FLAG_EXPAND;

        uint32_t source_size = BUF_READ(buf, uint32_t);
        buf.offset += 4;

        char *source = (char *)calloc(source_size + 1, 1);
        memcpy(source, BUF_OFFSET(buf), source_size);
        buf.offset += source_size;

        if (is_file)
        {
            ap_array_append_resize(root,
                                   &(APFile){strdup("*root*"), source,
                                             file_get_stat(source, NULL)},
                                   1);
        }
        else if (need_expand)
        {
            uint32_t entry_len = BUF_READ(buf, uint32_t);
            buf.offset += 4;
            APArrayT(APFile) *entries =
                ap_array_alloc(entry_len, sizeof(APFile));
            for (int j = 0; j < entry_len; j++)
            {
                uint32_t entry_size = BUF_READ(buf, uint32_t);
                buf.offset += 4;
                char *filename = (char *)calloc(entry_size + 1, 1);
                memcpy(filename, BUF_OFFSET(buf), entry_size);
                buf.offset += entry_size;
                ap_array_append_resize(
                    entries,
                    &(APFile){source, filename, file_get_stat(filename, NULL)},
                    1);
            }
            ap_array_append_resize(p->groups, &(APEntryGroup){source, entries},
                                   1);
        }
    }

    ap_array_append_resize(p->groups, &(APEntryGroup){"*root*", root}, 1);
}

/*
 * header:
 *     [version:i64, source_len:u32]
 * version uses semantic versioning
 * with format [major:u16 minor:u16 patch:u32]
 *
 * structure: [size:u64 flags:i64 source_size:u32 source:b, ...]
 * if flags specify file:
 *     structure: [size:u64 flags:i64 entry_size:u32 entry:b, ...]
 * if flags specify expand:
 *     structure: [size:u64 flags:i64 source_size:u32 source:b entry_len:u32
 * [entry_size:u32 entry:b, ...], ...]
 * */
void *ap_playlist_serialize(APPlaylist *p, bool expand_source, int *out_size)
{
    APDynBuf buf = {0};
    ap_dynbuf_init(&buf, 128);

    ap_dynbuf_ensure_free(&buf, 8);
    i16tob(BUF_OFFSET(buf) + 0, PLAYLIST_VERSION_MAJOR, true);
    i16tob(BUF_OFFSET(buf) + 2, PLAYLIST_VERSION_MINOR, true);
    i32tob(BUF_OFFSET(buf) + 4, PLAYLIST_VERSION_PATCH, true);
    buf.offset += 8;

    ap_dynbuf_ensure_free(&buf, 4);
    i32tob(BUF_OFFSET(buf), p->sources->len, true);
    buf.offset += 4;

    for (int i = 0; i < p->sources->len; i++)
    {
        APSource source = ARR_INDEX(p->sources, APSource *, i);
        int32_t flag = 0;
        if (source.is_file)
            flag |= PLAYLIST_FLAG_FILE;
        if (source.expand)
            flag |= PLAYLIST_FLAG_EXPAND;
        int start_offset = buf.offset;

        int size_offset = buf.offset;
        buf.offset += 8;

        ap_dynbuf_ensure_free(&buf, 8);
        i64tob(BUF_OFFSET(buf), flag, true);
        buf.offset += 8;

        int source_size = strlen(source.path);
        ap_dynbuf_ensure_free(&buf, 4);
        i64tob(BUF_OFFSET(buf), source_size, true);
        buf.offset += 4;

        ap_dynbuf_ensure_free(&buf, source_size);
        memcpy(BUF_OFFSET(buf), source.path, source_size);
        buf.offset += source_size;

        if (source.expand && !source.is_file)
        {
            int found = -1;
            if (p->groups)
                for (int j = 0; j < p->groups->len; j++)
                {
                    APEntryGroup group =
                        ARR_INDEX(p->groups, APEntryGroup *, j);
                    if (path_compare(group.name, source.path))
                    {
                        found = j;
                        break;
                    }
                }

            APArrayT(APFile) *entries = NULL;
            if (found < 0)
                entries = read_directory(source.path);
            else
                entries = ARR_INDEX(p->groups, APEntryGroup *, found).entries;

            if (entries)
            {
                ap_dynbuf_ensure_free(&buf, 4);
                i32tob(BUF_OFFSET(buf), entries->len, true);
                buf.offset += 4;

                for (int j = 0; j < entries->len; j++)
                {
                    APFile entry = ARR_INDEX(entries, APFile *, j);
                    int entry_size = strlen(entry.filename);

                    ap_dynbuf_ensure_free(&buf, 4);
                    i32tob(BUF_OFFSET(buf), entry_size, true);
                    buf.offset += 4;

                    ap_dynbuf_ensure_free(&buf, entry_size);
                    memcpy(BUF_OFFSET(buf), entry.filename, entry_size);
                    buf.offset += entry_size;
                }

                if (found < 0)
                    ap_playlist_entries_free(entries);
            }
        }

        i64tob(buf.data + size_offset, buf.offset - start_offset, true);
    }

    if (out_size)
        *out_size = buf.offset;

    return buf.data;
}

void ap_playlist_load(APPlaylist *p)
{
    if (!p->groups)
        ap_array_init(p->groups, 128, sizeof(APEntryGroup));
    APArrayT(APFile) *root = ap_array_alloc(128, sizeof(APFile));
    for (int i = 0; i < p->sources->len; i++)
    {
        APSource src = ARR_INDEX(p->sources, APSource *, i);
        if (src.is_file)
            ap_array_append_resize(root,
                                   &(APFile){strdup("*root*"), strdup(src.path),
                                             file_get_stat(src.path, NULL),
                                             mbstowchar(src.path, -1, NULL)},
                                   1);
        else
        {
            APArrayT(APFile) *entries = read_directory(src.path);
            // TODO: Report error
            if (!entries)
                continue;
            ap_array_append_resize(p->groups,
                                   &(APEntryGroup){src.path, entries}, 1);
        }
    }

    if (root->len == 0)
    {
        ap_array_free(&root);
        return;
    }

    ap_array_append_resize(p->groups, &(APEntryGroup){"*root*", root}, 1);
}
