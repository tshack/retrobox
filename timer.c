#include <stdlib.h>
#include "timer.h"

static double
get_time (Timer *timer)
{
#if defined (_WIN32)
    LARGE_INTEGER clock_count;
    QueryPerformanceCounter (&clock_count);
    return ((double) (clock_count.QuadPart)) / ((double) timer->clock_freq.QuadPart);
#else
    struct timeval tv;
    int rc;
    rc = gettimeofday (&tv, 0);
    return ((double) tv.tv_sec) + ((double) tv.tv_usec) / 1000000.;
#endif
}

void
timer_start (Timer *timer)
{
#if defined (_WIN32)
    QueryPerformanceFrequency (&timer->clock_freq);
#endif
    timer->start_time = get_time (timer);
}

double
timer_report (Timer *timer)
{
    double current_time;

    current_time = get_time (timer);
    return current_time - timer->start_time;
}

