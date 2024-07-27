#ifndef __ARRAY_H
#define __ARRAY_H

typedef struct array_t
{
    void *data;
    int length;
    int max_item;
    int item_size;
} array_t;

array_t array_create(int max_item, int item_size);
int array_append_static(array_t *arr, const void *mem, int item_count);
int array_append(array_t *arr, const void *mem, int item_count);

#endif /* __ARRAY_H */
