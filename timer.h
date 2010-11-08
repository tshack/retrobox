/*  This file is part of retrobox
    Copyright (C) 2010  Jamse A. Shackleford

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
