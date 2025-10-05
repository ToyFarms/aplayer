#ifndef __PATHLIB_H
#define __PATHLIB_H

#include "array.h"
#include "ds.h"
#include <stdbool.h>

typedef struct path_t
{
    str_t front;
    str_t back;
    array(strview_t) segments;
    bool is_abs;
} path_t;

path_t path_create(const char *pathstr);
void path_free(path_t *path);
// NOTE: normalizing path only format the single dot, duplicate and mixed slash
path_t *path_normalize(path_t *path);
// NOTE: resolving path will make the path absolute, while also collapsing the
// double dots
path_t *path_resolve(path_t *path);
str_t *path_render(path_t *path);

void path_segmentize(char *str, array_t *out);
void path_segmentize_insert(char *str, array_t *out, int index);
bool path_exists(char *path);
strview_t path_name(char *path);

#endif /* __PATHLIB_H */
