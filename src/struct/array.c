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
    arr.capacity = max_item;
    arr.item_size = item_size;
    arr.free = NULL;
    arr.freep = NULL;

    arr.data = malloc(max_item * item_size);
    if (arr.data == NULL)
        errno = -ENOMEM;

    return arr;
}

void array_free(array_t *arr)
{
    if (arr == NULL)
        return;

    if (arr->data)
        free(arr->data);
    arr->data = NULL;

    memset(arr, 0, sizeof(*arr));
}

int array_resize(array_t *arr, int new_max_item)
{
    assert(arr && arr->data && new_max_item > 0);
    arr->data = realloc(arr->data, new_max_item * arr->item_size);
    assert(arr->data);

    arr->capacity = new_max_item;

    return 0;
}

int array_append_static(array_t *arr, const void *mem, int item_count)
{
    assert(arr && mem && item_count > 0);
    if (arr->length + item_count > arr->capacity)
        return -ENOMEM;

    array_copy_buffer(arr, mem, item_count);

    return 0;
}

int array_append(array_t *arr, const void *mem, int item_count)
{
    assert(arr && mem && item_count > 0);
    if (item_count < 0 || arr->capacity <= 0)
        return -EINVAL;

    int prev_capacity = arr->capacity;
    while (arr->length + item_count > arr->capacity)
        arr->capacity *= 2;
    if (prev_capacity != arr->capacity)
        array_resize(arr, arr->capacity);
    array_copy_buffer(arr, mem, item_count);

    return 0;
}

int array_remove(array_t *arr, int offset, int item_count)
{
    assert(arr != NULL && arr->data != NULL);
    if (offset + item_count > arr->capacity)
        return -1;

    int leftover = arr->length - (offset + item_count);
    arr->length = offset + leftover;

    return 0;
}
