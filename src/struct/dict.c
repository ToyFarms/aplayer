#include "dict.h"
#include "logger.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

uint64_t hash_djb2(const char *buf, size_t size)
{
    uint64_t hash = 5381;
    for (size_t i = 0; i < size; i++)
        hash = ((hash << 5) + hash) + buf[i];

    return hash;
}

static array(dict_item) * buckets_alloc(int length)
{
    array(dict_item) *buckets = malloc(length * sizeof(array_t));
    if (buckets == NULL)
        return NULL;

    for (int i = 0; i < length; i++)
    {
        array(dict_item) *bkt = &buckets[i];

        *bkt = array_create(length > 32 ? length : 32, sizeof(dict_item));
        if (errno != 0)
        {
            log_error("Failed creating array for dict: %s\n", strerror(errno));
            free(buckets);
            return NULL;
        }
    }

    return buckets;
}

dict_t dict_create()
{
    errno = 0;
    dict_t dict = {0};

    dict.strcmp = strcmp;
    dict.hash = hash_djb2;
    dict.bucket_slot = 32;
    dict.buckets = buckets_alloc(dict.bucket_slot);
    if (dict.buckets == NULL)
        errno = -ENOMEM;

    return dict;
}

void dict_free(dict_t *dict)
{
    assert(dict);

    dict_clear(dict);
    free(dict->buckets);

    memset(dict, 0, sizeof(*dict));
}

void dict_clear(dict_t *dict)
{
    assert(dict);

    for (int i = 0; i < dict->bucket_slot; i++)
    {
        array(dict_item) bkt = dict->buckets[i];

        dict_item *item;
        ARR_FOREACH_BYREF(bkt, item, j)
        {
            if (item->self_owned)
                free(item->data);
            else
                FREE_EITHER(dict, item->data);
            item->data = NULL;

            free(item->key);
            item->key = NULL;
        }

        array_free(&bkt);
    }

    dict->length = 0;
}

static array_t *bkt_from_key(dict_t *dict, const char *key)
{
    int index = dict->hash(key, strlen(key)) % dict->bucket_slot;
    return &dict->buckets[index];
}

void dict_resize(dict_t *dict, int new_size)
{
    assert(dict && dict->buckets && new_size > 0);
    array(dict_item) *new = buckets_alloc(new_size);
    if (new == NULL)
    {
        errno = -ENOMEM;
        return;
    }

    for (int i = 0; i < dict->bucket_slot; i++)
    {
        array(dict_item) *bkt = &dict->buckets[i];
        if (bkt == NULL || bkt->length == 0)
            continue;

        dict_item item;
        ARR_FOREACH(*bkt, item, j)
        {
            int new_index = dict->hash(item.key, strlen(item.key)) % new_size;

            array(dict_item) *new_bkt = &new[new_index];
            array_append(new_bkt, &item, 1);
        }
    }

    free(dict->buckets);
    dict->buckets = new;
    dict->bucket_slot = new_size;
}

void dict_insert(dict_t *dict, const char *key, void *item)
{
    assert(dict && key);
    if (dict_getload(dict) > 0.75f)
        dict_resize(dict, dict->bucket_slot * 2);

    array_t *bkt = bkt_from_key(dict, key);

    dict_item *itm;
    ARR_FOREACH_BYREF(*bkt, itm, i)
    {
        // TODO: only workaround, make an insert function for array
        if (strcmp(itm->key, key) == 0)
        {
            array_remove(bkt, i, 1);
            dict->length -= 1;
            break;
        }
    }

    array_append(bkt, &(dict_item){.key = strdup(key), .data = item}, 1);
    dict->length += 1;
}

// TODO: add test for dict_insert_copy
void dict_insert_copy(dict_t *dict, const char *key, void *item, size_t size)
{
    assert(dict && key);

    if (dict_getload(dict) > 0.75f)
        dict_resize(dict, dict->bucket_slot * 2);

    array_t *bkt = bkt_from_key(dict, key);

    dict_item *itm;
    ARR_FOREACH_BYREF(*bkt, itm, i)
    {
        // TODO: only workaround, make an insert function for array
        if (strcmp(itm->key, key) == 0)
        {
            array_remove(bkt, i, 1);
            dict->length -= 1;
            break;
        }
    }

    void *mem = malloc(size);
    if (mem == NULL)
    {
        errno = -ENOMEM;
        return;
    }

    memcpy(mem, item, size);

    array_append(
        bkt, &(dict_item){.key = strdup(key), .data = mem, .self_owned = true},
        1);
    dict->length += 1;
}

void *dict_get(dict_t *dict, const char *key, void *default_return)
{
    assert(dict && key);

    array(dict_item) *bkt = bkt_from_key(dict, key);
    if (bkt == NULL || bkt->length == 0)
        goto def_ret;

    dict_item item;
    ARR_FOREACH(*bkt, item, i)
    {
        if (dict->strcmp(item.key, key) == 0)
            return item.data;
    }

def_ret:
    log_warning("Key '%s' does not exist in %p\n", key, (void *)dict);
    errno = -EINVAL;
    return default_return;
}

int dict_exists(dict_t *dict, const char *key)
{
    assert(dict && key);

    array(dict_item) *bkt = bkt_from_key(dict, key);
    if (bkt == NULL || bkt->length == 0)
        return 0;

    dict_item item;
    ARR_FOREACH(*bkt, item, i)
    {
        if (dict->strcmp(item.key, key) == 0)
            return 1;
    }

    return 0;
}

float dict_getload(dict_t *dict)
{
    assert(dict);

    if (dict->bucket_slot == 0)
        return 0;

    return (float)dict->length / (float)dict->bucket_slot;
}
