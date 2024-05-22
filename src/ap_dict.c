#include "ap_dict.h"

#include <stdlib.h>
#include <string.h>

static void ap_bucket_init(APBucket *bucket, const char *key, void *data);
static APBucket *ap_bucket_alloc(const char *key, void *data);
static void ap_bucket_linknext(APBucket *item, APBucket *next);
static APBucket *ap_bucket_get_tail(APBucket *bucket);
static APBucket *ap_bucket_tail_from_key(APBucket *bucket, const char *key);

void ap_dict_init(APDict *dict, int bucket_slot, AP_HASH_DEF(hash_fn))
{
    dict->bucket_slot = bucket_slot;
    dict->buckets = calloc(dict->bucket_slot, sizeof(*dict->buckets));
    dict->hash_fn = hash_fn;
}

APDict *ap_dict_alloc(int bucket_slot, AP_HASH_DEF(hash_fn))
{
    APDict *dict = calloc(1, sizeof(*dict));
    if (!dict)
        return NULL;

    ap_dict_init(dict, bucket_slot, hash_fn);

    return dict;
}

void ap_dict_free_items(APDict *dict)
{
    for (int i = 0; i < dict->bucket_slot; i++)
    {
        APBucket *bucket = &dict->buckets[i];
        free(bucket->key);
        bucket->key = NULL;

        if (!bucket->next)
            continue;

        APBucket *item = bucket->next;
        while (item)
        {
            APBucket *next = item->next;
            free(item->key);
            item->key = NULL;
            free(item);
            item = next;
        }
    }

    free(dict->buckets);
    dict->buckets = NULL;
}

void ap_dict_free(APDict **dict)
{
    if (!dict || !*dict)
        return;

    ap_dict_free_items(*dict);

    free(*dict);
    *dict = NULL;
}

void ap_dict_resize(APDict *dict, int new_size)
{
    if (new_size <= dict->bucket_slot)
        return;

    APBucket *buckets = calloc(new_size, sizeof(*buckets));

    for (int i = 0; i < dict->bucket_slot; i++)
    {
        APBucket bucket = dict->buckets[i];
        if (!bucket.key)
            continue;
        int index = dict->hash_fn(bucket.key) % new_size;
        buckets[index].key = bucket.key;
        buckets[index].next = bucket.next;
        buckets[index].data = bucket.data;
    }

    free(dict->buckets);
    dict->buckets = buckets;
    dict->bucket_slot = new_size;
}

void ap_dict_insert(APDict *dict, const char *key, void *data)
{
    if (AP_DICT_LOAD(dict) > 0.75f)
        ap_dict_resize(dict, dict->bucket_slot * 2);

    int index = dict->hash_fn(key) % dict->bucket_slot;
    APBucket *bucket = &dict->buckets[index];

    if (bucket->data == NULL && bucket->next == NULL)
        ap_bucket_init(bucket, key, data);
    else
    {
        ap_bucket_linknext(ap_bucket_tail_from_key(bucket, key),
                           ap_bucket_alloc(key, data));
    }
    dict->len++;
}

void *ap_dict_get(APDict *dict, const char *key)
{
    int index = dict->hash_fn(key) % dict->bucket_slot;
    APBucket *bucket = &dict->buckets[index];

    if (strcmp(bucket->key, key) == 0)
        return bucket->data;
    else if (bucket->next == NULL)
        return NULL;

    bucket = bucket->next;
    while (bucket)
    {
        if (strcmp(bucket->key, key) == 0)
            break;
        bucket = bucket->next;
    }

    if (bucket == NULL)
        return NULL;

    return bucket->data;
}

static void ap_bucket_init(APBucket *bucket, const char *key, void *data)
{
    bucket->key = strdup(key);
    bucket->data = data;
    bucket->next = NULL;
}

static APBucket *ap_bucket_alloc(const char *key, void *data)
{
    APBucket *bucket = calloc(1, sizeof(*bucket));
    ap_bucket_init(bucket, key, data);
    return bucket;
}

static void ap_bucket_linknext(APBucket *item, APBucket *next)
{
    item->next = next;
    next->next = NULL;
}

static APBucket *ap_bucket_get_tail(APBucket *bucket)
{
    while (bucket)
        bucket = bucket->next;
    return bucket;
}

static APBucket *ap_bucket_tail_from_key(APBucket *bucket, const char *key)
{
    while (bucket->next)
    {
        if (strcmp(bucket->key, key) == 0)
            return bucket;
        bucket = bucket->next;
    }

    return bucket;
}