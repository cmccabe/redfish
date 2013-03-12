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

#include "util/test.h"
#include "util/time.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

static int test_timespec_utils(void)
{
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	timespec_add_sec(&ts, 1);
	EXPECT_EQ(ts.tv_sec, 1);
	EXPECT_EQ(ts.tv_nsec, 0);
	timespec_add_nsec(&ts, 1000);
	EXPECT_EQ(ts.tv_sec, 1);
	EXPECT_EQ(ts.tv_nsec, 1000);
	timespec_add_nsec(&ts, 999999000);
	EXPECT_EQ(ts.tv_sec, 2);
	EXPECT_EQ(ts.tv_nsec, 0);
	return 0;
}

int main(void)
{
	time_t cur, next, after;

	EXPECT_ZERO(test_timespec_utils());
	cur = mt_time();
	next = cur + 1;
	mt_sleep_until(next);
	after = mt_time();
	EXPECT_GT(after, cur);
	EXPECT_GE(after, next);
	cur = mt_time();
	mt_msleep(1);
	after = mt_time();
	EXPECT_GE(after, cur);

	return EXIT_SUCCESS;
}
