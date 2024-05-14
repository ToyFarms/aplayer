#include "ap_array.h"

void ap_array_init(APArray *arr, int capacity, int item_size)
{
    arr->capacity = capacity;
    arr->data = calloc(capacity, item_size);
    arr->item_size = item_size;
}

APArray *ap_array_alloc(int capacity, int item_size)
{
    APArray *arr = (APArray *)calloc(1, sizeof(APArray));
    if (!arr)
        return NULL;

    ap_array_init(arr, capacity, item_size);

    return arr;
}

void ap_array_free(APArray **arr)
{
    if (!arr)
        return;
    if (!(*arr))
        return;

    free((*arr)->data);
    (*arr)->data = NULL;

    free(*arr);
    *arr = NULL;
}

#define ARR_OFFSET(array, offset)                                              \
    (((array)->data) + ((offset) * (array)->item_size))
#define ARR_APPEND(array, start, data, count)                                  \
    do                                                                         \
    {                                                                          \
        memcpy_s(ARR_OFFSET((array), (start)),                                 \
                 ((array)->capacity - (start)) * (array)->item_size, (data),   \
                 (count) * (array)->item_size);                                \
        if ((start) + (count) > (array)->len)                                  \
            (array)->len += ((start) + (count)) - (array)->len;                \
    } while (0)
#define ARR_MOVE(from_arr, from_start, count, to_arr, to_start)                \
    do                                                                         \
    {                                                                          \
        memmove_s(ARR_OFFSET((to_arr), (to_start)),                            \
                  ((to_arr)->capacity - (to_start)) * (to_arr)->item_size,     \
                  ARR_OFFSET((from_arr), (from_start)),                        \
                  (count) * (from_arr)->item_size);                            \
        if ((from_start) + (count) >= (from_arr)->len)                         \
            (from_arr)->len = from_start;                                      \
        if ((to_start) + (count) > (to_arr)->len)                              \
            (to_arr)->len += ((to_start) + (count)) - (to_arr)->len;           \
    } while (0)
#define ARR_CLEAR(array)                                                       \
    do                                                                         \
    {                                                                          \
        memset((array)->data, 0, (array)->capacity *(array)->item_size);       \
        (array)->len = 0;                                                      \
    } while (0)
#define ARR_FULL(array)      ((array)->len >= (array)->capacity)
#define ARR_FREESPACE(array) ((array)->capacity - (array)->len)

void ap_array_append_slide(APArray *arr, const void *data, int data_len)
{
    if (ARR_FREESPACE(arr) >= data_len)
        ARR_APPEND(arr, arr->len, data, data_len);
    else if (data_len > arr->capacity)
        ARR_APPEND(arr, 0, data, arr->capacity);
    else if (data_len > ARR_FREESPACE(arr))
    {
        int space_needed = data_len - ARR_FREESPACE(arr);
        ARR_MOVE(arr, space_needed, arr->len - space_needed, arr, 0);
        ARR_APPEND(arr, arr->capacity - data_len, data, data_len);
    }
}

void ap_array_append_wrap(APArray *arr, const void *data, int data_len,
                          bool reset_partial_fit)
{
    if (ARR_FREESPACE(arr) >= data_len)
        ARR_APPEND(arr, arr->len, data, data_len);
    else if (data_len > arr->capacity)
        ARR_APPEND(arr, 0, data, arr->capacity);
    else if (data_len > ARR_FREESPACE(arr))
    {
        if (reset_partial_fit || ARR_FULL(arr))
        {
            ARR_CLEAR(arr);
            ARR_APPEND(arr, 0, data, data_len);
        }
        else
            ARR_APPEND(arr, arr->len, data, ARR_FREESPACE(arr));
    }
}

void ap_array_append_resize(APArray *arr, const void *data, int data_len)
{
    if (ARR_FREESPACE(arr) >= data_len)
        ARR_APPEND(arr, arr->len, data, data_len);
    else
    {
        int capacity = arr->capacity;
        while (data_len >= capacity - arr->len)
            capacity *= 2;
        ap_array_resize(arr, capacity);
        ARR_APPEND(arr, arr->len, data, data_len);
    }
}

void ap_array_resize(APArray *arr, int new_capacity)
{
    arr->data = realloc(arr->data, new_capacity * arr->item_size);
    if (new_capacity > arr->capacity)
    {
        memset(ARR_OFFSET(arr, arr->capacity), 0,
               (new_capacity - arr->capacity) * arr->item_size);
    }
    arr->capacity = new_capacity;
    arr->len = arr->len > new_capacity ? new_capacity : arr->len;
}
