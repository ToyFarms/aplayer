#ifndef _LIBHELPER_H
#define _LIBHELPER_H

#include <stdint.h>
#include <stdio.h>
#include <libavutil/mem.h>
#include <libavutil/log.h>
#include <libavutil/time.h>
#include <stdbool.h>

#include "libos.h"
#include "libfile.h"

#define ARRLEN(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))
#define PR av_log(NULL, AV_LOG_INFO, "[PR] %s:%d\n", __FILE__, __LINE__);
#define PRM(x) av_log(NULL, AV_LOG_INFO, "[PRM] %s:%d - %s\n", __FILE__, __LINE__, x);
#define PRPTR(x) av_log(NULL, AV_LOG_INFO, "[PRPTR] %s:%d - %s: %p\n", __FILE__, __LINE__, #x, (void *)x);
#define AVLOG(fmt, ...) av_log(NULL, AV_LOG_INFO, "[AVLOG] %s:%d - "fmt, __FILE__, __LINE__, __VA_ARGS__)

static inline float lerpf(float v0, float v1, float t)
{
    return v0 + t * (v1 - v0);
}

static inline double lerp(double v0, double v1, double t)
{
    return v0 + t * (v1 - v0);
}

static inline int64_t us2ms(int64_t us)
{
    return us / 1000;
}

static inline int64_t ms2us(int64_t ms)
{
    return ms * 1000;
}

static inline int wrap_around(int n, int min, int max)
{
    return (((n - min) % (max - min)) + (max - min)) % (max - min) + min;
}

static inline float mapf(float value, float min, float max, float new_min, float new_max)
{
    if (min == max)
        return 0.0f;

    value = FFMIN(FFMAX(value, min), max);

    return (value - min) * (new_max - new_min) / (max - min) + min;
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

#ifdef AP_WINDOWS

#include <windows.h>
#include <shellapi.h>

#elif defined(AP_MACOS)
#elif defined(AP_LINUX)
#endif // AP_WINDOWS

void prepare_app_arguments(int *argc_ptr, char ***argv_ptr);
char *wchar2mbs(const wchar_t *strw);
wchar_t *mbs2wchar(char *str, size_t wsize, int *strwlen_out);

#endif // _LIBHELPER_H