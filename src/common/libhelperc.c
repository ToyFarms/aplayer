#include "libhelper.h"

static int prev_level;

void av_log_turn_off()
{
    prev_level = av_log_get_level();
    av_log_set_level(AV_LOG_QUIET);
}

void av_log_turn_on()
{
    av_log_set_level(prev_level);
}

void swap(void *a, void *b, size_t size)
{
    char temp[size];
    memcpy(temp, a, size);
    memcpy(a, b, size);
    memcpy(b, temp, size);
}

void reverse_array(void *array, size_t array_len, size_t element_size)
{
    for (size_t i = 0; i < array_len / 2; i++)
    {
        void *left = (char *)array + i * element_size;
        void *right = (char *)array + (array_len - i - 1) * element_size;
        swap(left, right, element_size);
    }
}

void shuffle_array(void *array, size_t array_len, size_t element_size)
{
    srand(av_gettime());

    for (int i = array_len - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);

        void *a = (char *)array + i * element_size;
        void *b = (char *)array + j * element_size;

        swap(a, b, element_size);
    }
}

bool file_compare_function(const void *a, const void *b)
{
    File *fa = (File *)a;
    File *fb = (File *)b;

    if (fa == NULL && fb == NULL)
        return true;

    if (fa == NULL || fb == NULL)
        return false;

    wchar_t *aw = mbs2wchar(fa->path, 1024, NULL);
    wchar_t *bw = mbs2wchar(fb->path, 1024, NULL);

    bool res = path_comparew(aw, bw);

    free(aw);
    free(bw);

    return res;
}

bool array_find(const void *array,
                int array_size,
                int element_size,
                const void *target,
                bool(compare_function)(const void *a,
                                       const void *b),
                int *out_index)
{
    for (int i = 0; i < array_size; i++)
    {
        if (compare_function((const char *)array + (i * element_size), target))
        {
            if (out_index)
                *out_index = i;
            return true;
        }
    }

    return false;
}

bool is_numeric(char *str)
{
    for (char *c = str; *c != '\0'; c++)
    {
        if (!isdigit(*c))
            return false;
    }
    return true;
}

Array *array_alloc(int capacity, int item_size)
{
    Array *arr = (Array *)calloc(1, sizeof(Array));

    arr->capacity = capacity;
    arr->data = (void *)calloc(capacity, item_size);
    arr->item_size = item_size;

    return arr;
}

void array_free(Array **arr)
{
    if (!arr || !(*arr))
        return;

    free((*arr)->data);
    (*arr)->data = NULL;

    free(*arr);
    *arr = NULL;
}

#define ARR_OFFSET(array, offset) (((array)->data) + ((offset) * (array)->item_size))
#define ARR_APPEND(array, start, data, count)                        \
    do                                                               \
    {                                                                \
        memcpy_s(ARR_OFFSET((array), (start)),                       \
                 ((array)->capacity - (start)) * (array)->item_size, \
                 (data),                                             \
                 (count) * (array)->item_size);                      \
        if ((start) + (count) > (array)->len)                        \
            (array)->len += ((start) + (count)) - (array)->len;      \
    } while (0)
#define ARR_MOVE(from_arr, from_start, count, to_arr, to_start)            \
    do                                                                     \
    {                                                                      \
        memmove_s(ARR_OFFSET((to_arr), (to_start)),                        \
                  ((to_arr)->capacity - (to_start)) * (to_arr)->item_size, \
                  ARR_OFFSET((from_arr), (from_start)),                    \
                  (count) * (from_arr)->item_size);                        \
        if ((from_start) + (count) >= (from_arr)->len)                     \
            (from_arr)->len = from_start;                                  \
        if ((to_start) + (count) > (to_arr)->len)                          \
            (to_arr)->len += ((to_start) + (count)) - (to_arr)->len;       \
                                                                           \
    } while (0)
#define ARR_CLEAR(array)                                                 \
    do                                                                   \
    {                                                                    \
        memset((array)->data, 0, (array)->capacity *(array)->item_size); \
        (array)->len = 0;                                                \
    } while (0)
#define ARR_FULL(array) ((array)->len == (array)->capacity)
#define ARR_FREESPACE(array) ((array)->capacity - (array)->len)

void array_append_slide(Array *arr, const void *data, int data_len)
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

void array_append_wrap(Array *arr, const void *data, int data_len, bool reset_partial_fit)
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
