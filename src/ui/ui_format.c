#include "ui_format.h"

#include <stdbool.h>
#include <stdio.h>

void ui_format_time(char *out, size_t cap, uint64_t us)
{
    if (cap == 0)
        return;

    uint64_t sec = (us / 1000000ULL) % 60, min = (us / 60000000ULL) % 60;
    uint64_t hrs = (us / 3600000000ULL) % 24, days = us / 86400000000ULL;
    size_t n = 0;

    if (days >= 1)
        n += (size_t)snprintf(out + n, cap - n,
                              "%llu:", (unsigned long long)days);
    if (n >= cap)
        return;

    bool have_hour = hrs >= 1 || days >= 1;
    if (have_hour)
        n += (size_t)snprintf(out + n, cap - n, days >= 1 ? "%02llu:" : "%llu:",
                              (unsigned long long)hrs);
    if (n >= cap)
        return;

    snprintf(out + n, cap - n, have_hour ? "%02llu:%02llu" : "%llu:%02llu",
             (unsigned long long)min, (unsigned long long)sec);
}
