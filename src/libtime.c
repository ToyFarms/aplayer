#include "libtime.h"

Time time_from_ns(double ns)
{
    Time t;

    t.ns = fmod(ns, 1e3);
    t.us = fmod(ns / 1e3, 1e3);
    t.ms = fmod(ns / 1e6, 1e3);
    t.s = fmod(ns / 1e9, 60.0);
    t.m = fmod(ns / (1e9 * 60.0), 60.0);
    t.h = fmod(ns / (1e9 * 3600.0), 24.0);
    t.d = floor(ns / (1e9 * 3600.0 * 24.0));

    return t;
}

Time time_from_us(double us)
{
    Time t;

    t.ns = fmod(us * 1e3, 1e3);
    t.us = fmod(us, 1e3);
    t.ms = fmod(us / 1e3, 1e3);
    t.s = fmod(us / 1e6, 60.0);
    t.m = fmod(us / (1e6 * 60.0), 60.0);
    t.h = fmod(us / (1e6 * 3600.0), 24.0);
    t.d = floor(us / (1e6 * 3600.0 * 24.0));

    return t;
}

Time time_from_ms(double ms)
{
    Time t;

    t.ns = fmod(ms * 1e6, 1e3);
    t.us = fmod(ms * 1e3, 1e3);
    t.ms = fmod(ms, 1e3);
    t.s = fmod(ms / 1e3, 60.0);
    t.m = fmod(ms / (1e3 * 60.0), 60.0);
    t.h = fmod(ms / (1e3 * 3600.0), 24.0);
    t.d = floor(ms / (1e3 * 3600.0 * 24.0));

    return t;
}

Time time_from_s(double s)
{
    Time t;

    t.ns = fmod(s * 1e9, 1e3);
    t.us = fmod(s * 1e6, 1e3);
    t.ms = fmod(s * 1e3, 1e3);
    t.s = fmod(s, 60.0);
    t.m = fmod(s / 60.0, 60.0);
    t.h = fmod(s / 3600.0, 24.0);
    t.d = floor(s / 3600.0 * 24.0);

    return t;
}

Time time_from_m(double m)
{
    Time t;

    t.ns = fmod(m * 60.0 * 1e9, 1e3);
    t.us = fmod(m * 60.0 * 1e6, 1e3);
    t.ms = fmod(m * 60.0 * 1e3, 1e3);
    t.s = fmod(m * 60.0, 60.0);
    t.m = fmod(m, 60.0);
    t.h = fmod(m / 60.0, 24.0);
    t.d = floor(m / (60.0 * 24.0));

    return t;
}

Time time_from_h(double h)
{
    Time t;

    t.ns = fmod(h * 3600.0 * 1e9, 1e3);
    t.us = fmod(h * 3600.0 * 1e6, 1e3);
    t.ms = fmod(h * 3600.0 * 1e3, 1e3);
    t.s = fmod(h * 3600.0, 60.0);
    t.m = fmod(h * 60.0, 60.0);
    t.h = fmod(h, 24.0);
    t.d = floor(h / 24.0);

    return t;
}

Time time_from_d(double d)
{
    Time t;

    t.ns = fmod(d * 24.0 * 3600.0 * 1e9, 1e3);
    t.us = fmod(d * 24.0 * 3600.0 * 1e6, 1e3);
    t.ms = fmod(d * 24.0 * 3600.0 * 1e3, 1e3);
    t.s = fmod(d * 24.0 * 3600.0, 60.0);
    t.m = fmod(d * 24.0 * 60.0, 60.0);
    t.h = fmod(d * 24.0, 24.0);
    t.d = d;

    return t;
}