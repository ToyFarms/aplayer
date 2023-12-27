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

    return fa->path == fb->path;
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
