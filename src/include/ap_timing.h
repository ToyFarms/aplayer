#ifndef __AP_TIMING_H
#define __AP_TIMING_H

#define AP_CLOCK_PRINT_ELAPSED(clock)                                          \
    printf(#clock " = %fs", ap_clock_get_elapsed(clock))
#define AP_CLOCK_BEGIN(name)                                                   \
    APClock *name = ap_clock_alloc();                                          \
    ap_clock_begin(name)
#define AP_CLOCK_END(name)                                                     \
    ap_clock_end(name);                                                        \
    AP_CLOCK_PRINT_ELAPSED(name);                                              \
    ap_clock_free(name)

typedef struct APClock APClock;

APClock *ap_clock_alloc();
void ap_clock_free(APClock *c);
void ap_clock_begin(APClock *c);
void ap_clock_end(APClock *c);
double ap_clock_get_elapsed(APClock *c);

#endif /* __AP_TIMING_H */
