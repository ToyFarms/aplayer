#ifndef __DICT_H
#define __DICT_H

#include "struct.h"

typedef struct bucket_t
{
    void *data;
} bucket_t;

typedef struct dict_t
{
    bucket_t *buckets;
    int length;
    int capacity;

    FREE_DEF(free);
    FREEP_DEF(freep);
} dict_t;

dict_t dict_create();
void dict_free(dict_t *dict);

#endif /* __DICT_H */
