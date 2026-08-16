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
    bool self_owned; // whether the `data` is allocated internally
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

typedef struct
{
    dict_t *d;
    int bucket_idx;
    int slot_idx;
} dict_iter_t;

static inline dict_item *dict_iter_next(dict_iter_t *it)
{
    while (it->bucket_idx < it->d->bucket_slot)
    {
        array_t bkt = it->d->buckets[it->bucket_idx];
        if (it->slot_idx < bkt.length)
        {
            dict_item *item = &ARR_AS(bkt, dict_item)[it->slot_idx];
            it->slot_idx++;
            return item;
        }
        it->bucket_idx++;
        it->slot_idx = 0;
    }
    return NULL;
}

#define DICT_FOREACH(mykey, myitem, myi, mydict)                               \
    for (dict_iter_t __it = {(mydict), 0, 0};;)                                \
        for (int myi = -1, __go = 1; __go; __go = 0)                           \
            for (dict_item * __item;                                           \
                 (__item = dict_iter_next(&__it)) != NULL ? (myi++, 1) : 0;)     \
                for (int __once = 1; __once; __once = 0)                       \
                    for (char *mykey = __item->key, *item = 0; __once;         \
                         __once = 0, myitem = __item->data)
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
