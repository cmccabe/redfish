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

#ifndef REDFISH_UTIL_PLATFORM_READDIR_DOT_H
#define REDFISH_UTIL_PLATFORM_READDIR_DOT_H

struct dirent;
struct redfish_dirp;

/** Open a directory for reading. Similar to opendir(3)
 *
 * @param name		Name of directory to open
 * @param dp		(out param) on success; a pointer to the opened
 *			directory
 *
 * @return		0 on success; error code otherwise
 */
extern int do_opendir(const char *name, struct redfish_dirp** dp);

/** Read another entry from a directory.
 *
 * This is similar to readdir, but guaranteed to be thread-safe on every
 * platform.
 *
 * @param dp		directory to read
 *
 * @return		the next directory entry, or NULL if there are no more.
 */
extern struct dirent *do_readdir(struct redfish_dirp *dp);

/** Close a directory and free the associated resources
 *
 * @param dp		directory to close
 */
extern void do_closedir(struct redfish_dirp *dp);

#endif
