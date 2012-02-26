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

#ifndef REDFISH_UTIL_PLATFORM_THREAD_ID_DOT_H
#define REDFISH_UTIL_PLATFORM_THREAD_ID_DOT_H

#include <stdint.h> /* for uint32_t */

/** Create a unique thread ID.
 *
 * Only call this once per thread.  It _may_ return a new value each time you
 * call it.  On certain platforms, it will return the kernel's thread id, which
 * may be useful for debugging.
 *
 * @return		a unique thread id
 */
extern uint32_t create_unique_thread_id(void);

#endif
