#include "pathlib.h"

#include <assert.h>
#include <unistd.h>

// TODO: come back to this, and change fs that use char to use path_t
// TODO: figure out how to structure this with cross-platform in mind

path_t path_create(const char *path_str)
{
    path_t path = {0};
    path.front = str_new(path_str);
    path.back = str_create();
    path.segments = array_create(16, sizeof(strview_t));

    path.is_abs =
        path.front.len >= 1 && (path_str[0] == '/' || path_str[0] == '\\');

    path_segmentize(path.front.buf, &path.segments);

    return path;
}

void path_free(path_t *path)
{
    if (path == NULL)
        return;

    str_free(&path->front);
    str_free(&path->back);
    array_free(&path->segments);
}

path_t *path_normalize(path_t *path)
{
    strview_t *view;
    ARR_FOREACH_BYREF(path->segments, view, i)
    {
        if (view->len == 1 && view->buf[0] == '.')
            array_remove(&path->segments, i--, 1);
    }

    if (path->segments.length == 0)
        array_append(&path->segments, &(strview_t){".", 1}, 1);

    return path;
}

// TODO: resolve path link & junction
path_t *path_resolve(path_t *path)
{
    if (!path->is_abs)
    {
        char cwd[1024];
        getcwd(cwd, 1024);
        path_segmentize_insert(cwd, &path->segments, 0);
        path->is_abs = true;
    }

    strview_t *view;
    ARR_FOREACH_BYREF(path->segments, view, i)
    {
        if (view->len == 1 && view->buf[0] == '.')
            array_remove(&path->segments, i--, 1);
        else if (view->len == 2 && view->buf[0] == '.' && view->buf[1] == '.')
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

    return path;
}

static void path_segmentize_generic(char *str, array_t *out, int index)
{
    strview_t seg = {.buf = str, .len = 0};
    char c = 0;
    while ((c = *str++))
    {
        if (c == '/' || c == '\\')
        {
            if (seg.len > 0)
            {
                index < 0 ? array_append(out, &seg, 1)
                          : array_insert(out, &seg, 1, index++);
                seg.buf += seg.len;
                seg.len = 0;
            }
            seg.buf++;
        }
        else
            seg.len++;
    }
    if (seg.len > 0)
    {
        index < 0 ? array_append(out, &seg, 1)
                  : array_insert(out, &seg, 1, index++);
    }
}

void path_segmentize(char *str, array_t *out)
{
    path_segmentize_generic(str, out, -1);
}

void path_segmentize_insert(char *str, array_t *out, int index)
{
    path_segmentize_generic(str, out, index);
}

str_t *path_render(path_t *path)
{
    path->back.len = 0;
    if (path->is_abs)
        str_catch(&path->back, '/');

    strview_t *seg;
    ARR_FOREACH_BYREF(path->segments, seg, i)
    {
        str_catlen(&path->back, seg->buf, seg->len);
        if (i != path->segments.length - 1)
            str_catch(&path->back, '/');
    }

    str_t temp = path->front;
    path->front = path->back;
    path->back = temp;

    return &path->front;
}
