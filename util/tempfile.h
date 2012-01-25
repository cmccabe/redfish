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
#ifndef REDFISH_UTIL_TEMPFILE_H
#define REDFISH_UTIL_TEMPFILE_H

/** Create a temporary directory
 *
 * @param tempdir	(out param) name buffer
 * @param name_max	length of tempdir buffer
 * @param mode		mode to use in mkdir
 *
 * @return		0 on success; error code otherwise
 */
int get_tempdir(char *tempdir, int name_max, int mode);

/** Register a temporary directory to be deleted at the end of the program
 *
 * @param tempdir	The tempdir to register
 *
 * @return		0 on success; error code otherwise
 */
int register_tempdir_for_cleanup(const char *tempdir);

/** Unregister a temporary directory to be deleted at the end of the program
 *
 * @param tempdir	The tempdir to unregister
 */
void unregister_tempdir_for_cleanup(const char *tempdir);

/** Remove a tempdir
 *
 * @param tempdir	The tempdir to remove
 */
void remove_tempdir(const char *tempdir);

#endif
