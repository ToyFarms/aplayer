#include "array.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void array_copy_buffer(array_t *arr, const void *mem, int item_count)
{
    memcpy(arr->data + (arr->length * arr->item_size), mem,
           item_count * arr->item_size);
    arr->length += item_count;
}

array_t array_create(int max_item, int item_size)
{
    assert(max_item > 0 && item_size > 0);

    array_t arr = {0};
    arr.length = 0;
    arr.max_item = max_item;
    arr.item_size = item_size;

    arr.data = malloc(max_item * item_size);
    if (arr.data == NULL)
        errno = -ENOMEM;

    return arr;
}

int array_resize(array_t *arr, int new_max_item)
{
    assert(arr && arr->data && new_max_item > 0);
    arr->data = realloc(arr->data, new_max_item * arr->item_size);
    assert(arr->data);

    arr->max_item = new_max_item;

    return 0;
}

int array_append_static(array_t *arr, const void *mem, int item_count)
{
    assert(arr && mem && item_count > 0);
    if (arr->length + item_count > arr->max_item)
        return -ENOMEM;

    array_copy_buffer(arr, mem, item_count);

    return 0;
}

int array_append(array_t *arr, const void *mem, int item_count)
{
    assert(arr && mem && item_count > 0);
    if (item_count < 0 || arr->max_item <= 0)
        return -EINVAL;

    while (arr->length + item_count > arr->max_item)
        arr->max_item *= 2;
    array_resize(arr, arr->max_item);
    array_copy_buffer(arr, mem, item_count);

    return 0;
}
