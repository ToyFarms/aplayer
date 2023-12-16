#ifndef _LIBHELPER_H
#define _LIBHELPER_H

#include <stdint.h>
#include <libavutil/mem.h>

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

#include <windows.h>
#include <shellapi.h>
/* Will be leaked on exit */
static char **win32_argv_utf8 = NULL;
static int win32_argc = 0;

/**
 * Prepare command line arguments for executable.
 * For Windows - perform wide-char to UTF-8 conversion.
 * Input arguments should be main() function arguments.
 * @param argc_ptr Arguments number (including executable)
 * @param argv_ptr Arguments list.
 */
void prepare_app_arguments(int *argc_ptr, char ***argv_ptr);

#endif // _LIBHELPER_H