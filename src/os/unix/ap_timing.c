#include "ap_timing.h"

#include <stdlib.h>
#include <time.h>

typedef struct APClock
{
    struct timespec start, end;
} APClock;

APClock *ap_clock_alloc()
{
    return calloc(1, sizeof(APClock));
}

void ap_clock_free(APClock *c)
{
    free(c);
}

void ap_clock_begin(APClock *c)
{
    clock_gettime(CLOCK_MONOTONIC, &c->start);
}

void ap_clock_end(APClock *c)
{
    clock_gettime(CLOCK_MONOTONIC, &c->end);
}

double ap_clock_get_elapsed(APClock *c)
{
    return (c->end.tv_sec - c->start.tv_sec) +
           (c->end.tv_nsec - c->start.tv_nsec) / 1e9;
}
