/*  This file is part of retrobox
    Copyright (C) 2010  James A. Shackleford

    retrobox is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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

