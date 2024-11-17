#ifndef __UTILS_H
#define __UTILS_H

/*
 * This file is intended for storing "temporary" function that doesn't have its
 * own category (yet)
 * */

#include <stdint.h>
#include <time.h>

// TODO: move time related function into a separate file

#ifdef _WIN32
#  error Function not implemented
#elif defined(__linux__)
static inline uint64_t get_time_ms()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((uint64_t)now.tv_sec * 1000000000UL + (uint64_t)now.tv_nsec) /
           1000000;
}
static inline uint64_t get_time_ns()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((uint64_t)now.tv_sec * 1000000000UL + (uint64_t)now.tv_nsec);
}
#endif

void print_raw(const char *str);

#endif /* __UTILS_H */
