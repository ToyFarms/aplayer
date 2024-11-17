#ifndef __ARRAY_H
#define __ARRAY_H

typedef struct array_t
{
    void *data;
    int length;
    int capacity;
    int item_size;
} array_t;

#define array(...)     array_t
#define ARR_AS(arr, T) ((T *)((arr).data))
#define ARR_FOREACH(arr, elm, i)                                               \
    for (int i = 0;                                                            \
         i < (arr).length && (elm = ARR_AS(arr, typeof(elm))[i], 1); i++)
#define ARR_FOREACH_BYREF(arr, elm, i)                                           \
    for (int i = 0;                                                            \
         i < (arr).length && (elm = &(ARR_AS(arr, typeof(*elm)))[i]); i++)

array_t array_create(int max_item, int item_size);
void array_free(array_t *arr);
int array_append_static(array_t *arr, const void *mem, int item_count);
int array_append(array_t *arr, const void *mem, int item_count);
int array_insert(array_t *arr, const void *mem, int item_count, int index);
int array_remove(array_t *arr, int index, int item_count);

#endif /* __ARRAY_H */
