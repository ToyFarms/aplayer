#include "array.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void *offset(array_t *arr)
{
    return arr->data + (arr->length * arr->item_size);
}

static void *offsetfrom(array_t *arr, size_t from)
{
    return arr->data + (from * arr->item_size);
}

static void array_copy_buffer(array_t *arr, const void *mem, int item_count)
{
    memcpy(offset(arr), mem, item_count * arr->item_size);
    arr->length += item_count;
}

array_t array_create(int max_item, int item_size)
{
    assert(max_item > 0 && item_size > 0);

    errno = 0;
    array_t arr = {0};

    arr.length = 0;
    arr.capacity = max_item;
    arr.item_size = item_size;

    arr.data = calloc(max_item, item_size);
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

void array_resize(array_t *arr, int new_max_item)
{
    if (new_max_item == arr->capacity)
        return;

    assert(arr && arr->data && new_max_item > 0);
    arr->data = realloc(arr->data, new_max_item * arr->item_size);
    assert(arr->data);

    if (new_max_item > arr->capacity)
    {
        memset(offsetfrom(arr, arr->capacity), 0,
               (new_max_item - arr->capacity) * arr->item_size);
    }

    arr->capacity = new_max_item;
}

int array_append_static(array_t *arr, const void *mem, int item_count)
{
    assert(arr && mem && item_count > 0);
    if (arr->length + item_count > arr->capacity)
        return -ENOMEM;
    else if (arr == NULL || item_count <= 0 || mem == NULL)
        return -EINVAL;

    array_copy_buffer(arr, mem, item_count);

    return 0;
}

int array_append(array_t *arr, const void *mem, int item_count)
{
    assert(arr && mem && item_count > 0);
    if (item_count < 0 || arr->capacity <= 0)
        return -EINVAL;
    else if (arr == NULL || item_count <= 0 || mem == NULL)
        return -EINVAL;

    int capacity = arr->capacity;
    while (arr->length + item_count > capacity)
        capacity *= 2;
    array_resize(arr, capacity);

    array_copy_buffer(arr, mem, item_count);

    return 0;
}

int array_append_slide(array_t *arr, const float *mem, int item_count)
{
    assert(arr && mem && item_count > 0);
    if (item_count < 0 || arr->capacity <= 0)
        return -EINVAL;
    else if (arr == NULL || item_count <= 0 || mem == NULL)
        return -EINVAL;

    if (item_count >= arr->capacity)
    {
        memcpy(arr->data, mem + (item_count - arr->capacity) * arr->item_size,
               arr->capacity * arr->item_size);
        arr->length = arr->capacity;
    }
    else if (arr->length + item_count <= arr->capacity)
    {
        array_copy_buffer(arr, mem, item_count);
    }
    else
    {
        int overflow = (arr->length + item_count) - arr->capacity;
        memmove(arr->data, offsetfrom(arr, overflow),
                (arr->length - overflow) * arr->item_size);
        memcpy(offsetfrom(arr, arr->length - overflow), mem,
               item_count * arr->item_size);
        arr->length = arr->capacity;
    }

    return 0;
}

int array_insert(array_t *arr, const void *mem, int item_count, int index)
{
    assert(arr && mem && item_count > 0 && index >= 0);
    if (item_count < 0 || arr->capacity <= 0 || index < 0)
        return -EINVAL;

    int index_space = index > arr->capacity ? index : 0;

    int capacity = arr->capacity;
    while (arr->length + item_count + index_space > capacity)
        capacity *= 2;
    array_resize(arr, capacity);

    if (index < arr->length)
    {
        memmove(offsetfrom(arr, index + item_count), offsetfrom(arr, index),
                (arr->length - index) * arr->item_size);
    }

    memcpy(offsetfrom(arr, index), mem, item_count * arr->item_size);

    arr->length += item_count;

    return 0;
}

int array_remove(array_t *arr, int index, int item_count)
{
    assert(arr && arr->data);

    if (index + item_count > arr->capacity)
        item_count = arr->capacity - index;

    int move_size = (arr->length - (index + item_count)) * arr->item_size;

    if (move_size > 0)
        memmove(arr->data + index * arr->item_size,
                arr->data + (index + item_count) * arr->item_size, move_size);

    arr->length -= item_count;

    return 0;
}

static void swap_element(array_t *arr, int a, int b)
{
    unsigned char *x = arr->data + (a * arr->item_size);
    unsigned char *y = arr->data + (b * arr->item_size);

    for (int i = 0; i < arr->item_size; i++)
    {
        x[i] ^= y[i];
        y[i] ^= x[i];
        x[i] ^= y[i];
    }
}

void array_shuffle(array_t *arr)
{
    assert(arr && arr->data);
    if (arr->length < 2)
        return;

    for (int i = arr->length - 1; i > 0; --i)
    {
        int j = rand() % (i + 1);
        if (i == j)
            continue;
        swap_element(arr, i, j);
    }
}

void array_reverse(array_t *arr)
{
    assert(arr && arr->data);
    if (arr->length < 2)
        return;

    for (int i = 0; i < arr->length / 2; ++i)
        swap_element(arr, i, arr->length - 1 - i);
}
