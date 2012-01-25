/*
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

#ifndef REDFISH_UTIL_PLATFORM_SIGNAL_DOT_H
#define REDFISH_UTIL_PLATFORM_SIGNAL_DOT_H

#include <unistd.h> /* for size_t */

/** Given the third argument passed to a signal handler, pull out some useful
 * platform-specific data.
 *
 * One example would be the instruction pointer at the time that the signal was
 * delivered.
 *
 * @param data		The third argument passed to a signal handler
 * @param out		(out param) The output buffer
 * @param out_len	Length of the output buffer
 */
extern void signal_analyze_plat_data(const void *data, char *out,
			size_t out_len);

#endif
