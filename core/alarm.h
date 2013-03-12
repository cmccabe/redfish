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

#ifndef REDFISH_CORE_ALARM_DOT_H
#define REDFISH_CORE_ALARM_DOT_H

#include <time.h> /* for time_t */

/** Set an alarm which will trigger at a given monotonic time
 *
 * @param time		The monotonic expiration time
 * @param death_msg	Message to print if the timer expires.  The caller is
 *			responsible for managing the memory associated with this
 *			string.
 * @param timer		(out param) The alarm
 *
 * @return		0 on success; error code otherwise
 */
extern int mt_set_alarm(time_t time, const char *death_msg, timer_t *timer);

/** Deactivate a previously scheduled alarm
 *
 * @param timer		The alarm to deactivate
 *
 * @return		0 on success; error code otherwise
 */
extern int mt_deactivate_alarm(timer_t timer);

#endif
