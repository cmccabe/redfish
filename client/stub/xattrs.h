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

#ifndef REDFISH_CLIENT_STUB_XATTRS_DOT_H
#define REDFISH_CLIENT_STUB_XATTRS_DOT_H

#include <unistd.h> /* for size_t */

/** Check if the filesystem at 'base' supports xattrs, by performing a
 * write/read cycle.
 *
 * This function is not re-entrant.
 *
 * @param base		Base path to check
 *
 * @return		0 if the filesystem supports xattrs; an error code if
 *			an error was encountered during the test.
 */
extern int check_xattr_support(const char *base);

/** Get an xattr as a C-style string
 *
 * @param epath		file path
 * @param xname		xattr name
 * @param buf_sz	buffer size to use when reading xattr
 * @param x		(out param) will be filled with dynamically allocated
 *			string on success.
 *
 * @return		0 on success; error code otherwise.
 */
extern int xgets(const char *epath, const char *xname, size_t buf_sz, char **x);

/** Get an xattr as a C-style string
 *
 * @param fd		file descriptor
 * @param xname		xattr name
 * @param buf_sz	buffer size to use when reading xattr
 * @param x		(out param) will be filled with dynamically allocated
 *			string on success.
 *
 * @return		0 on success; error code otherwise.
 */
extern int fxgets(int fd, const char *xname, size_t buf_sz, char **x);

/** Set an xattr as a C-style string
 *
 * @param epath		file path
 * @param xname		xattr name
 * @param s		xattr value
 *
 * @return		0 on success; error code otherwise.
 */
extern int xsets(const char *epath, const char *xname, const char *s);

/** Set an xattr as a C-style string
 *
 * @param fd		file descriptor
 * @param xname		xattr name
 * @param s		xattr value
 *
 * @return		0 on success; error code otherwise.
 */
extern int fxsets(int fd, const char *xname, const char *s);

/** Get an xattr as an int
 *
 * @param epath		file path
 * @param xname		xattr name
 * @param base		numerical base to use (usually 10)
 * @param x		(out param) will be filled with the int on success.
 *
 * @return		0 on success; error code otherwise.
 */
extern int xgeti(const char *epath, const char *xname, int base, int *x);

/** Get an xattr as an int
 *
 * @param fd		file descriptor
 * @param xname		xattr name
 * @param base		numerical base to use (usually 10)
 * @param x		(out param) will be filled with the int on success.
 *
 * @return		0 on success; error code otherwise.
 */
extern int fxgeti(int fd, const char *xname, int base, int *x);

/** Set an xattr as an int
 *
 * @param epath		file path
 * @param xname		xattr name
 * @param base		numerical base to use (usually 10)
 * @param x		the int
 *
 * @return		0 on success; error code otherwise.
 */
extern int xseti(const char *epath, const char *xname, int base, int i);

/** Set an xattr as an int
 *
 * @param fd		file descriptor
 * @param xname		xattr name
 * @param base		numerical base to use (usually 10)
 * @param x		the int
 *
 * @return		0 on success; error code otherwise.
 */
extern int fxseti(int fd, const char *xname, int base, int i);

#endif
