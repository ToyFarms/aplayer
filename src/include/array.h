#ifndef __ARRAY_H
#define __ARRAY_H

#include "struct.h"

typedef struct array_t
{
    void *data;
    int length;
    int capacity;
    int item_size;

    FREE_DEF(free);
    FREEP_DEF(freep);
} array_t;

#define array(...)           array_t
#define ARR_IDX(T, arr, idx) (((T *)(arr).data)[idx])
#define ARR_MAP(T, arr, code)                                                  \
    do                                                                         \
    {                                                                          \
        for (unsigned int _item_i = 0; _item_i < (arr).length; _item_i++)      \
        {                                                                      \
            T _item = ARR_IDX(T, arr, _item_i);                                \
            code;                                                              \
        }                                                                      \
    } while (0)
#define ARR_REM(T, arr, start, count)                                          \
    do                                                                         \
    {                                                                          \
        for (unsigned int _item_i = (start);                                   \
             _item_i < (arr).length && _item_i < (start) + (count);           \
             _item_i++)                                                        \
        {                                                                      \
            void *_item = ARR_IDX(T, arr, _item_i);                            \
            FREE_EITHER(arr, _item);                                           \
        }                                                                      \
        array_remove((arr), (start), (count));                                 \
    } while (0);

array_t array_create(int max_item, int item_size);
void array_free(array_t *arr);
int array_append_static(array_t *arr, const void *mem, int item_count);
int array_append(array_t *arr, const void *mem, int item_count);
int array_remove(array_t *arr, int offset, int item_count);

#endif /* __ARRAY_H */
