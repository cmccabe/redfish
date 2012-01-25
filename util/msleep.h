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

#ifndef REDFISH_UTIL_SLEEP_DOT_H
#define REDFISH_UTIL_SLEEP_DOT_H

/** Sleep for a given number of milliseconds.
 * Unlike sleep(3), this is guaranteed not to use signals.
 *
 * @param milli		Number of milliseconds to sleep
 */
void do_msleep(int milli);

#endif
