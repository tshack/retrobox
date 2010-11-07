#ifndef _timer_h_
#define _timer_h_

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

typedef struct timer_struct Timer;
struct timer_struct {
    double start_time;
#ifdef _WIN32
    LARGE_INTEGER clock_freq;
#endif
};

#if defined __cplusplus
extern "C" {
#endif

void timer_start (Timer *timer);
double timer_report (Timer *timer);

#if defined __cplusplus
}
#endif



#endif
