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

#ifndef REDFISH_UTIL_PATH_DOT_H
#define REDFISH_UTIL_PATH_DOT_H

#include <unistd.h> /* for size_t */

/** Canonicalize paths so that they have only one slash between path
 * components, and do not end with a slash.
 *
 * @param path		The (absolute) path
 *
 * @return		0 on success; -ENOTSUP if the path was not absolute
 */
int canonicalize_path(char *path);

/** Get the name of the directory enclosing a file
 *
 * @param path		The path
 * @param dir		(out param) The directory enclosing 'path'
 * @param dir_len	Length of the dir buffer
 */
void do_dirname(const char *fname, char *dname, size_t dname_len);

#endif
