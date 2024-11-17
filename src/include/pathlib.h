#ifndef __PATHLIB_H
#define __PATHLIB_H

#include "array.h"
#include "ds.h"
#include <stdbool.h>

typedef struct path_t
{
    string_t front;
    string_t back;
    array(stringview_t) segments;
} path_t;

path_t path_create(const char *pathstr);
void path_free(path_t *path);
// NOTE: normalizing path only format the single dot, duplicate and mixed slash
path_t *path_normalize(path_t *path);
// NOTE: resolving path will make the path absolute, while also collapsing the
// double dots
path_t *path_resolve(path_t *path);
bool path_is_absolute(const path_t *path);

#endif /* __PATHLIB_H */
