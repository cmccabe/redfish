/*
 * Copyright 2011-2012 the Redfish authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util/time.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NSEC_PER_SEC 1000000000

time_t mt_time(void)
{
	int res;
	struct timespec ts;
       
	res = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (res)
		abort();
	return ts.tv_sec;
}

void mt_sleep_until(time_t until)
{
	int res;
	struct timespec ts, rts;

	memset(&ts, 0, sizeof(ts));
	ts.tv_sec = until;
	memset(&rts, 0, sizeof(rts));
	while (1) {
		res = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
				&ts, &rts);
		if (res)
			abort();
		if ((rts.tv_sec == 0) && (rts.tv_nsec == 0))
			break;
		/* We were interrupted by a signal.  Sleep the remaining amount
		 * of time. */
	}
}

void mt_msleep(int milli)
{
	int sec, res;
	struct timespec ts, rts;

	memset(&ts, 0, sizeof(ts));
	sec = milli / 1000;
	milli -= (sec * 1000);
	ts.tv_sec = sec;
	ts.tv_nsec = milli * 1000;
	memset(&rts, 0, sizeof(rts));
	while (1) {
		res = clock_nanosleep(CLOCK_MONOTONIC, 0,
				&ts, &rts);
		if (res)
			abort();
		if ((rts.tv_sec == 0) && (rts.tv_nsec == 0))
			break;
		/* We were interrupted by a signal.  Sleep the remaining amount
		 * of time. */
		ts.tv_sec = rts.tv_sec;
		ts.tv_nsec = rts.tv_nsec;
	}
}

void timespec_add_sec(struct timespec *ts, time_t sec)
{
	ts->tv_sec += sec;
}

void timespec_add_nsec(struct timespec *ts, int nsec)
{
	ts->tv_nsec += nsec;
	if (ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_nsec -= NSEC_PER_SEC;
		ts->tv_sec++;
	}
}
