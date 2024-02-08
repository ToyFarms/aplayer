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

SlidingArray *sarray_alloc(int capacity, int item_size)
{
    SlidingArray *sarr = (SlidingArray *)calloc(1, sizeof(SlidingArray));

    sarr->capacity = capacity;
    sarr->data = (void *)malloc(capacity * item_size);
    sarr->item_size = item_size;

    return sarr;
}

#define PTR_OFFSET(ptr, n) (ptr + (n))

void sarray_append(SlidingArray *sarr, const void *data, int data_len)
{
    if (sarr->capacity - sarr->len >= data_len)
    {
        // enough space to fit data
        memcpy_s(PTR_OFFSET(sarr->data, sarr->len * sarr->item_size), sarr->capacity - sarr->len, data, data_len);
        sarr->len += data_len;
    }
    else if (data_len > sarr->capacity)
    {
        // data is bigger than capacity, discard anything above capacity
        memcpy_s(sarr->data, sarr->capacity, data, sarr->capacity);
        sarr->len = sarr->capacity;
    }
    else if (data_len > sarr->capacity - sarr->len)
    {
        // slide array to make space for new data
        int space_needed = data_len - (sarr->capacity - sarr->len);
        memmove_s(sarr->data, sarr->capacity - data_len, PTR_OFFSET(sarr->data, space_needed), sarr->len - space_needed);
        memcpy_s(PTR_OFFSET(sarr->data, sarr->capacity - data_len), sarr->capacity - data_len, data, data_len);
        sarr->len = sarr->capacity;
    }
}

void sarray_free(SlidingArray **sarr)
{
    if (!sarr || !(*sarr))
        return;
    
    free((*sarr)->data);
    (*sarr)->data = NULL;

    free(*sarr);
    *sarr = NULL;
}