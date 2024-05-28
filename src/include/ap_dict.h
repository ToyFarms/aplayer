#ifndef __AP_DICT_H
#define __AP_DICT_H

#include <stdint.h>

#define AP_HASH_DEF(name) uint64_t (*name)(const char *)
#define AP_STRCMP_DEF(name) int (*name)(const char *, const char *)
#define AP_DICT_LOAD(dict)                                                     \
    ((dict)->bucket_slot == 0                                                  \
         ? 0.0f                                                                \
         : (float)(dict)->len / (float)(dict)->bucket_slot)

typedef struct APBucket
{
    char *key;
    void *data;
    struct APBucket *next;
} APBucket;

typedef struct APDict
{
    int bucket_slot;
    int len;
    APBucket *buckets;
    AP_HASH_DEF(hash_fn);
    AP_STRCMP_DEF(keycmp_fn);
} APDict;

uint64_t ap_hash_djb2(const char *str);

void ap_dict_init(APDict *dict, int bucket_slot, AP_HASH_DEF(hash_fn));
APDict *ap_dict_alloc(int bucket_slot, AP_HASH_DEF(hash_fn));
void ap_dict_free_items(APDict *dict);
void ap_dict_free(APDict **dict);
void ap_dict_resize(APDict *dict, int new_size);
void ap_dict_insert(APDict *dict, const char *key, void *data);
void *ap_dict_get(APDict *dict, const char *key);

#endif /* __AP_DICT_H */
