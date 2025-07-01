#include "clock.h"
#include <pthread.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#  define PLATFORM_WINDOWS
#  include <mmsystem.h>
#  include <windows.h>
#  pragma comment(lib, "winmm.lib")
#else
#  define PLATFORM_POSIX
#  include <time.h>
#  include <unistd.h>
#endif

void clock_init(clock_highres_t *clk)
{
#ifdef PLATFORM_WINDOWS
    timeBeginPeriod(1);

    QueryPerformanceFrequency(&clk->freq);
    clk->inited = 1;

    LARGE_INTEGER now_cnt;
    QueryPerformanceCounter(&now_cnt);
    clk->last_time_ns =
        (uint64_t)((now_cnt.QuadPart * 1000000000ULL) / clk->freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    clk->last_time_ns =
        (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif

    clk->next_frame_time_ns = clk->last_time_ns;
}

void clock_free(clock_highres_t *clk)
{
#ifdef PLATFORM_WINDOWS
    if (clk->inited)
    {
        timeEndPeriod(1);
        clk->inited = 0;
    }
#endif
}

uint64_t clock_now_ns(clock_highres_t *clk)
{
#ifdef PLATFORM_WINDOWS
    if (!clk->inited)
    {
        QueryPerformanceFrequency(&clk->freq);
        clk->inited = 1;
    }
    LARGE_INTEGER now_cnt;
    QueryPerformanceCounter(&now_cnt);
    return (uint64_t)((now_cnt.QuadPart * 1000000000ULL) / clk->freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

uint64_t clock_get_delta_ns(clock_highres_t *clk)
{
    uint64_t now_ns = clock_now_ns(clk);
    uint64_t dt_ns = now_ns - clk->last_time_ns;
    clk->last_time_ns = now_ns;
    return dt_ns;
}

void clock_sleep(clock_highres_t *clk, uint64_t sleep_ns)
{
#ifdef PLATFORM_WINDOWS
    DWORD ms = (DWORD)(sleep_ns / 1000000ULL);
    if (ms > 0)
        Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(sleep_ns / 1000000000ULL);
    ts.tv_nsec = (long)(sleep_ns % 1000000000ULL);
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
#endif
}

void clock_throttle(clock_highres_t *clk, uint64_t target_fps)
{
    if (target_fps == 0ULL)
    {
        uint64_t now_ns = clock_now_ns(clk);
        clk->last_time_ns = now_ns;
        clk->next_frame_time_ns = now_ns;
        return;
    }
    uint64_t period_ns = 1000000000ULL / target_fps;

    uint64_t now_ns = clock_now_ns(clk);

    if (clk->next_frame_time_ns <= clk->last_time_ns)
        clk->next_frame_time_ns = clk->last_time_ns + period_ns;

    if (now_ns < clk->next_frame_time_ns)
    {
        clock_sleep(clk, clk->next_frame_time_ns - now_ns);
        clk->last_time_ns = clk->next_frame_time_ns;
        clk->next_frame_time_ns += period_ns;
    }
    else
    {
        while (now_ns >= clk->next_frame_time_ns)
            clk->next_frame_time_ns += period_ns;
        clk->last_time_ns = now_ns;
    }
}

static clock_highres_t s_global_clk;
static pthread_once_t s_global_init_once = PTHREAD_ONCE_INIT;

static void _global_clock_init(void)
{
    clock_init(&s_global_clk);
}

uint64_t gclock_now_ns(void)
{
    pthread_once(&s_global_init_once, _global_clock_init);
    return clock_now_ns(&s_global_clk);
}
