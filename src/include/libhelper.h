#ifndef _LIBHELPER_H
#define _LIBHELPER_H

#include <stdint.h>
#include <libavutil/mem.h>
#include <libavutil/log.h>

#define ARRLEN(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))
#define PR printf("%s:%d\n", __FILE__, __LINE__);
#define PRM(x) printf("%s:%d - %s\n", __FILE__, __LINE__, x);
#define PRPTR(x) printf("%s:%d - %s: %p\n", __FILE__, __LINE__, #x, (void *)x);

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

void av_log_turn_off();
void av_log_turn_on();

#ifdef AP_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif // AP_WINDOWS

void prepare_app_arguments(int *argc_ptr, char ***argv_ptr);

#endif // _LIBHELPER_H