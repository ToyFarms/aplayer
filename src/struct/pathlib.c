#include "pathlib.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// TODO: figure out how to structure this with cross-platform in mind
static void concat_segment(path_t *path, bool absolute);
static void swap_buffer(path_t *path);
void path_segments(char *str, array_t *out, int index);

path_t path_create(const char *path_str)
{
    path_t path = {0};
    path.front = string_create();
    path.back = string_create();
    path.segments = array_create(16, sizeof(stringview_t));

    string_cat(&path.front, path_str);

    return path;
}

void path_free(path_t *path)
{
    if (path == NULL)
        return;

    string_free(&path->front);
    string_free(&path->back);
    array_free(&path->segments);
}

path_t *path_normalize(path_t *path)
{
    path->segments.length = 0;
    path_segments(path->front.buf, &path->segments, 0);

    stringview_t *strview;
    ARR_FOREACH_BYREF(path->segments, strview, i)
    {
        if (strview->len == 1 && strview->buf[0] == '.')
            array_remove(&path->segments, i--, 1);
    }

    if (path->segments.length == 0)
    {
        path->back.len = 0;
        string_cat(&path->back, path_is_absolute(path) ? "/." : ".");
        swap_buffer(path);
    }
    else
        concat_segment(path, path_is_absolute(path));

    return path;
}

// TODO: resolve path link & junction
path_t *path_resolve(path_t *path)
{
    path->segments.length = 0;
    path_segments(path->front.buf, &path->segments, 0);

    if (!path_is_absolute(path))
    {
        char cwd[1024];
        getcwd(cwd, 1024);
        path_segments(cwd, &path->segments, 0);
    }

    stringview_t *strview;
    ARR_FOREACH_BYREF(path->segments, strview, i)
    {
        if (strview->len == 1 && strview->buf[0] == '.')
            array_remove(&path->segments, i--, 1);
        else if (strview->len == 2 && strview->buf[0] == '.' &&
                 strview->buf[1] == '.')
        {
            if (i == 0)
            {
                array_remove(&path->segments, i, 1);
                i -= 1;
            }
            else
            {
                array_remove(&path->segments, i - 1, 2);
                i -= 2;
            }
        }
    }

    concat_segment(path, true);
    return path;
}

void path_segments(char *str, array_t *out, int index)
{
    enum state_type
    {
        STATE_READ,
        STATE_FLUSH,
        STATE_END,
    } state = STATE_READ;

    char c = *str;
    stringview_t seg = {.buf = str, .len = 0};
    while (true)
    {
        switch (state)
        {
        case STATE_READ:
            if (c != '\0')
                c = *str++;
            if (c == '/' || c == '\\' || c == '\0')
                state = (c == '\0') ? STATE_END : STATE_FLUSH;
            else
                seg.len++;
            break;
        case STATE_FLUSH:
            if (seg.len > 0)
            {
                array_insert(out, &seg, 1, index++);
                seg.buf += seg.len;
                seg.len = 0;
            }

            seg.buf++;
            state = STATE_READ;
            break;
        case STATE_END:
            if (seg.len > 0)
                state = STATE_FLUSH;
            else
                goto exit;
            break;
        }
    }
exit:;
}

static void swap_buffer(path_t *path)
{
    string_t temp = path->front;
    path->front = path->back;
    path->back = temp;
}

static void concat_segment(path_t *path, bool absolute)
{
    path->back.len = 0;
    if (absolute)
        string_catch(&path->back, '/');

    stringview_t *seg;
    ARR_FOREACH_BYREF(path->segments, seg, i)
    {
        string_catlen(&path->back, seg->buf, seg->len);
        if (i != path->segments.length - 1)
            string_catch(&path->back, '/');
    }

    swap_buffer(path);
}

bool path_is_absolute(const path_t *path)
{
    return path->front.len > 0 &&
           (path->front.buf[0] == '/' || path->front.buf[0] == '\\');
}
