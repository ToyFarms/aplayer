#ifndef _LIBHELPER_H
#define _LIBHELPER_H

#include <stdint.h>
#include <stdio.h>
#include <libavutil/mem.h>
#include <libavutil/log.h>
#include <libavutil/time.h>
#include <stdbool.h>
#include <ctype.h>

#include "libos.h"
#include "libfile.h"

#define ARRLEN(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))
#define PR av_log(NULL, AV_LOG_INFO, "[PR] %s:%d\n", __FILE__, __LINE__);
#define PRM(x) av_log(NULL, AV_LOG_INFO, "[PRM] %s:%d - %s\n", __FILE__, __LINE__, x);
#define PRPTR(x) av_log(NULL, AV_LOG_INFO, "[PRPTR] %s:%d - %s: %p\n", __FILE__, __LINE__, #x, (void *)x);
#define AVLOG(fmt, ...) av_log(NULL, AV_LOG_INFO, "[AVLOG] %s:%d - " fmt, __FILE__, __LINE__, __VA_ARGS__)
#define AVLOGR(fmt, ...) av_log(NULL, AV_LOG_INFO, fmt, __VA_ARGS__)

#define LERP(v0, v1, t) ((v0) + (t) * ((v1) - (v0)))
#define USTOMS(us) ((us) / 1000)
#define MSTOUS(ms) ((ms) * 1000)
#define WRAP_AROUND(_n, _min, _max) (((((_n) - (_min)) % ((_max) - (_min))) + ((_max) - (_min))) % ((_max) - (_min)) + (_min))

static inline double map(double value, double _min, double _max, double new_min, double new_max)
{
    value = FFMIN(FFMAX(value, _min), _max);

    double v = (value - _min) / (_max - _min) * (new_max - new_min) + new_min;

    return new_min < new_max
               ? FFMIN(FFMAX(v, new_min), new_max)
               : FFMIN(FFMAX(v, new_max), new_min);
}

static inline float mapf(float value, float _min, float _max, float new_min, float new_max)
{
    value = FFMIN(FFMAX(value, _min), _max);

    float v = (value - _min) / (_max - _min) * (new_max - new_min) + new_min;

    return new_min < new_max
               ? FFMIN(FFMAX(v, new_min), new_max)
               : FFMIN(FFMAX(v, new_max), new_min);
}

static inline double map3(double value,
                          double min, double mid, double max,
                          double new_min, double new_mid, double new_max)
{
    return value <= mid
               ? map(value, min, mid, new_min, new_mid)
               : map(value, mid, max, new_mid, new_max);
}

static inline float map3f(float value,
                          float min, float mid, float max,
                          float new_min, float new_mid, float new_max)
{
    return value <= mid
               ? mapf(value, min, mid, new_min, new_mid)
               : mapf(value, mid, max, new_mid, new_max);
}

void av_log_turn_off();
void av_log_turn_on();
void reverse_array(void *array, size_t array_len, size_t element_size);
void shuffle_array(void *array, size_t array_len, size_t element_size);

bool file_compare_function(const void *a, const void *b);
bool array_find(const void *array,
                int array_size,
                int element_size,
                const void *target,
                bool(compare_function)(const void *a,
                                       const void *b),
                int *out_index);
bool is_numeric(char *str);

typedef struct SlidingArray
{
    int capacity;    
    int len;
    void *data;
    int item_size;
} SlidingArray;

SlidingArray *sarray_alloc(int capacity, int item_size);
void sarray_append(SlidingArray *sarr, const void *data, int data_len);
void sarray_free(SlidingArray **sarr);

#ifdef AP_WINDOWS

#include <windows.h>
#include <shellapi.h>

#elif defined(AP_MACOS)
#elif defined(AP_LINUX)
#endif // AP_WINDOWS

void prepare_app_arguments(int *argc_ptr, char ***argv_ptr);
char *wchar2mbs(const wchar_t *strw);
wchar_t *mbs2wchar(const char *str, size_t wsize, int *strwlen_out);

#endif // _LIBHELPER_H