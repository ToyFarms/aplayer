#ifndef _LIBTIME_H
#define _LIBTIME_H

#include <stdint.h>
#include <math.h>

typedef struct Time
{
    double d;
    double h;
    double m;
    double s;
    double ms;
    double us;
    double ns;
} Time;

Time time_from_ns(double ns);
Time time_from_us(double us);
Time time_from_ms(double ms);
Time time_from_s(double s);
Time time_from_m(double m);
Time time_from_h(double h);
Time time_from_d(double d);

#endif /* _LIBTIME_H */ 