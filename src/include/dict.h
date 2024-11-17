#ifndef __DICT_H
#define __DICT_H

#include "array.h"
#include "struct.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct dict_item
{
    char *key;
    void *data;
    bool self_owned; // whether the `data` is malloc'ed internally
} dict_item;

typedef uint64_t (*dict_hash_fn)(const char *key, size_t len);
typedef int (*dict_strcmp_fn)(const char *a, const char *b);

typedef struct dict_t
{
    array(dict_item) * buckets;
    int bucket_slot;
    int length;

    free_fn free;
    freep_fn freep;
    dict_hash_fn hash;
    dict_strcmp_fn strcmp;
} dict_t;

#define DICT_FOREACH(key, item, i, dict)                                       \
    for (int __i = 0, __attribute__((unused)) i = -1;                          \
         __i < (dict)->bucket_slot; __i++)                                     \
    {                                                                          \
        __attribute__((unused)) char *key = NULL;                              \
        array_t __bkt = (dict)->buckets[__i];                                  \
        dict_item *__item;                                                     \
        ARR_FOREACH_BYREF(__bkt, __item, __j)                                  \
        {                                                                      \
            i++;                                                               \
            key = __item->key;                                                 \
            item = __item->data;

#define DICT_FOREACH_END                                                       \
    }                                                                          \
    }

dict_t dict_create();
void dict_free(dict_t *dict);
void dict_clear(dict_t *dict);
void dict_resize(dict_t *dict, int new_size);
void dict_insert(dict_t *dict, const char *key, void *item);
void dict_insert_copy(dict_t *dict, const char *key, void *item, size_t size);
void *dict_get(dict_t *dict, const char *key, void *default_return);
float dict_getload(dict_t *dict);
int dict_exists(dict_t *dict, const char *key);

#endif /* __DICT_H */
