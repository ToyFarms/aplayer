#include "ap_timing.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

typedef struct APClock
{
    LARGE_INTEGER frequency, start, end;
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
    QueryPerformanceFrequency(&c->frequency);
    QueryPerformanceCounter(&c->start);
}

void ap_clock_end(APClock *c)
{
    QueryPerformanceCounter(&c->end);
}

double ap_clock_get_elapsed(APClock *c)
{
    return (double)(c->end.QuadPart - c->start.QuadPart) / c->frequency.QuadPart;
}
