/*
 * vim: ts=8:sw=8:tw=79:noet
 * 
 * Copyright 2012 the Redfish authors
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

#include "core/alarm.h"
#include "util/error.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int mt_set_alarm(time_t at, const char *death_msg, timer_t *timer)
{
	struct itimerspec ispec;
	struct sigevent evp;
	int ret;

	evp.sigev_value.sival_ptr = (void*)death_msg;
	evp.sigev_notify = SIGEV_SIGNAL;
	evp.sigev_signo = SIGALRM;
	ret = timer_create(CLOCK_MONOTONIC, &evp, timer);
	if (ret)
		return FORCE_NEGATIVE(ret);
	memset(&ispec, 0, sizeof(ispec));
	ispec.it_value.tv_sec = at;
	ret = timer_settime(*timer, TIMER_ABSTIME, &ispec, NULL);
	if (ret) {
		timer_delete(*timer);
		return FORCE_NEGATIVE(ret);
	}
	return 0;
}

int mt_deactivate_alarm(timer_t timer)
{
	int ret;

	ret = timer_delete(timer);
	if (ret)
		return FORCE_NEGATIVE(ret);
	return 0;
}
