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

#ifndef REDFISH_UTIL_CLOCK_DOT_H
#define REDFISH_UTIL_CLOCK_DOT_H

#include <time.h> /* for time_t */

/** Get the monotonic time_t
 *
 * @return		The current monotonic time.  This is NOT the same as the
 *			wall-clock time returned by time() and friends.
 */
extern time_t mt_time(void);

/** Sleep until a given monotonic time_t.
 *
 * - Does not use SIGALARM
 * - Will sleep for the full amount of time despite interruptions by signals
 *
 * @param until		__Monotonic__ time to sleep until
 */
extern void mt_sleep_until(time_t until);

/** Sleep for a given number of milliseconds.
 *
 * - Does not use SIGALARM
 * - Will sleep for the full amount of time despite interruptions by signals
 *
 * @param milli		Number of milliseconds to sleep
 */
extern void mt_msleep(int milli);

#endif
