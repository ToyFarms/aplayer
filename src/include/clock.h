#ifndef __CLOCK_H
#define __CLOCK_H

#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#  include <winnt.h>
#else
#endif

typedef struct clock_highres_t
{
    uint64_t last_time_ns;
    uint64_t next_frame_time_ns;

#ifdef PLATFORM_WINDOWS
    LARGE_INTEGER freq;
    int inited;
#endif
} clock_highres_t;

#define S2NS(n)  ((uint64_t)(n) * 1000000000ULL)
#define MS2NS(n) ((uint64_t)(n) * 1000000ULL)

void clock_init(clock_highres_t *clk);
void clock_free(clock_highres_t *clk);
uint64_t clock_now_ns(clock_highres_t *clk);
uint64_t clock_get_delta_ns(clock_highres_t *clk);
void clock_sleep(clock_highres_t *clk, uint64_t sleep_ns);
void clock_throttle(clock_highres_t *clk, uint64_t target_fps);
uint64_t gclock_now_ns(void);

#endif /* __CLOCK_H */
