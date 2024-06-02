#ifndef __AP_ARRAY_H
#define __AP_ARRAY_H

#include <stdbool.h>

typedef struct APArray
{
    int capacity;
    int len;
    void *data;
    int item_size;
} APArray;

#define ARR_CAST(array, T)     ((T)((array)->data))
#define ARR_INDEX(array, T, i) (ARR_CAST((array), T)[(i)])
#define APArrayT(...) APArray

/* initialize APArray */
void ap_array_init(APArray *arr, int capacity, int item_size);
/* allocate and initialize new array */
APArray *ap_array_alloc(int capacity, int item_size);
/* free and set arr to NULL */
void ap_array_free(APArray **arr);
/* will discard the first n bytes and 'slide' the rest of the array to make space
 * for new data */
void ap_array_append_slide(APArray *arr, const void *data, int data_len);
/* if reset_partial_fit is true, it will free the array then append it at the start;
 *
 * if not, it will append what fit into the array
 * */
void ap_array_append_wrap(APArray *arr, const void *data, int data_len,
                          bool reset_partial_fit);
/* will realloc the array by 2x if the data doesn't fit */
void ap_array_append_resize(APArray *arr, const void *data, int data_len);
/* realloc the array */
void ap_array_resize(APArray *arr, int new_capacity);

#endif /* __AP_ARRAY_H */
