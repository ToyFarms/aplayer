#include "dict.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

// TODO: implement

dict_t dict_create()
{
    dict_t dict = {0};

    dict.capacity = 32;
    dict.buckets = malloc(dict.capacity * sizeof(*dict.buckets));
    if (dict.buckets == NULL)
        errno = -ENOMEM;

    return dict;
}

void dict_free(dict_t *dict)
{
    assert(dict != NULL && dict->buckets != NULL);
    for (int i = 0; i < dict->length; i++)
    {
        bucket_t bkt = dict->buckets[i];
        FREE_EITHER(dict, bkt.data);
    }
    free(dict->buckets);
    dict->buckets = NULL;
    dict->capacity = 0;
    dict->length = 0;
}

void dict_insert(void *item)
{
    
}
