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

#ifndef REDFISH_UTIL_DIR_DOT_H
#define REDFISH_UTIL_DIR_DOT_H

#include <unistd.h>

/** Create a directory if it doesn't already exist
 *
 * @param dir_name	Directory name
 * @param mode		Mode to use when creating directory
 * @param err		Error buffer. Will be set if an error is encountered.
 * @param err_len	length of error buffer
 */
void do_mkdir(const char *dir_name, int mode, char *err, size_t err_len);

/** Create a directory and all ancestor directories
 *
 * @param path		Directory name
 * @param mode		Mode to use when creating directories
 *
 * @return		0 on success; error code on failure
 * 			Some directories may be created even on failure.
 */
int do_mkdir_p(const char *path, int mode);

#endif
