/*
 * Copyright 2011-2012 the RedFish authors
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

#ifndef REDFISH_UTIL_USERNAME_DOT_H
#define REDFISH_UTIL_USERNAME_DOT_H

#include <unistd.h> /* for size_t */

/** Find the current username
 *
 * @param out		(out param) the username
 * @param out_len	length of the out buffer
 *
 * @return		0 on success; -ENAMETOOLONG if the buffer is too short,
 * 			-ENOSYS if the username could not be found.
 */
extern int get_current_username(char *out, size_t out_len);

#endif