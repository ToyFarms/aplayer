#include "ap_playlist.h"

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

void ap_playlist_free_member(APPlaylist *p)
{
    if (!p)
        return;

    for (int i = 0; i < p->groups->len; i++)
    {
        APEntryGroup group = ARR_INDEX(p->groups, APEntryGroup *, i);
        free(group.id);
        group.id = NULL;
        for (int j = 0; j < group.entries->len; j++)
        {
            APFile file = ARR_INDEX(group.entries, APFile *, j);
            free(file.directory);
            file.directory = NULL;
            free(file.filename);
            file.filename = NULL;
        }
        ap_array_free(&group.entries);
    }

    ap_array_free(&p->groups);
    for (int i = 0; i < p->sources->len; i++)
    {
        APSource source = ARR_INDEX(p->sources, APSource *, i);
        free(source.path);
        source.path = NULL;
    }
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
#define BUF_OFFSET(buf) ((buf).data + (buf).offset)

void ap_playlist_deserialize(APPlaylist *p, void *_buf, int buf_size)
{
    APDynBuf buf = {_buf, 0, buf_size};
    uint16_t major = BUF_READ(buf, uint16_t);
    buf.offset += 2;
    uint16_t minor = BUF_READ(buf, uint16_t);
    buf.offset += 2;
    uint32_t patch = BUF_READ(buf, uint32_t);
    buf.offset += 4;
    printf("version %d.%d.%d\n", major, minor, patch);

    uint32_t source_len = BUF_READ(buf, uint32_t);
    buf.offset += 4;
    printf("source: %d\n", source_len);
    for (int i = 0; i < source_len; i++)
    {
        uint64_t size = BUF_READ(buf, uint64_t);
        buf.offset += 8;
        int64_t flag = BUF_READ(buf, int64_t);
        buf.offset += 8;
        bool is_file = flag & PLAYLIST_FLAG_FILE;

        char *source = (char *)calloc((size - 8) + 1, 1);
        memcpy(source, BUF_OFFSET(buf), size - 8);

        buf.offset += size;

        printf("{\n");
        printf("    size: %lld, is_file: %d\n", size, is_file);
        printf("    source: %s\n", source);
        printf("}\n");

        free(source);
    }
}

/*
 * header:
 *     [version:i64, source_len:u32]
 * version uses semantic versioning
 * with format [major:u16 minor:u16 patch:u32]
 *
 * structure: [size:u64 flags:i64 source:b, ...]
 * if flags specify file:
 *     structure: [size:u64 flags:i64 entry:b, ...]
 * if flags specify expand:
 *     structure: [size:u64 flags:i64 source:b [entry_size:u32 entry:b, ...], ...]
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
        uint64_t size = 0;
        int32_t flag = 0;
        if (source.is_file)
            flag |= PLAYLIST_FLAG_FILE;

        int size_offset = buf.offset;
        buf.offset += 8;

        ap_dynbuf_ensure_free(&buf, 8);
        i64tob(BUF_OFFSET(buf), flag, true);
        buf.offset += 8;
        size += 8;

        int source_len = strlen(source.path);
        ap_dynbuf_ensure_free(&buf, source_len);
        memcpy(BUF_OFFSET(buf), source.path, source_len);
        buf.offset += source_len;
        size += source_len;

        printf("%lld\n", size);
        i64tob(buf.data + size_offset, size, true);
        buf.offset += 8;
    }

    if (out_size)
        *out_size = buf.offset;

    return buf.data;
}

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

    ap_array_append_resize(p->groups, &(APEntryGroup){strdup("*root*"), root}, 1);
}
