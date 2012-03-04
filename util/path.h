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
 * The only exception is the root directory, which is always canonicalized as
 * a single slash.
 *
 * @param path		The (absolute) path
 *
 * @return		0 on success; -ENOTSUP if the path was not absolute
 */
extern int canonicalize_path(char *path);

/** Canonicalize paths so that they have only one slash between path
 * components, and do not end with a slash.
 *
 * The only exception is the root directory, which is always canonicalized as
 * a single slash.
 *
 * @param dst		(out param) The canonicalized path
 * @param dst_len	length of dst
 * @param src		The (absolute) input path
 *
 * @return		Length of dst_len on success.
 *			-ENOTSUP if the path was not absolute.
 * 			-ENAMETOOLONG if the canonicalized path did not fit in
 * 			dst.
 */
extern int canonicalize_path2(char *dst, size_t dst_len, const char *src);

/** Append a relative path to a canonical path.
 *
 * Since the root directory ends in a slash, you can't just append a slash and a
 * relative path to a canonical path and expect to get another canonical path.
 * Instead, use this function.
 *
 * @param base		(out param) the canonicalized path to append to
 * @param base_len	length of 'base'
 * @param suffix	the relative path to append
 *
 * @return		0 on success; -ENAMETOOLONG if there isn't enough space
 */
extern int canon_path_append(char *out, size_t out_len, const char *suffix);

/** Append a charater path to a canonical path.
 *
 * Put a single character at the end of the path.  For the root path (/),
 * replace the root path with the suffix.
 *
 * @param base		(out param) the canonicalized path to append to
 * @param base_len	length of 'base'
 * @param suffix	the relative path to append
 *
 * @return		0 on success; -ENAMETOOLONG if there isn't enough space
 */
extern int canon_path_add_suffix(char *base, size_t out_len, char suffix);

/** Get the name of the directory enclosing a file
 *
 * @param path		The path
 * @param dir		(out param) The directory enclosing 'path'
 * @param dir_len	Length of the dir buffer
 */
extern void do_dirname(const char *fname, char *dname, size_t dname_len);

/** Get the last path component of a path
 *
 * Note: The last path component of / is considered to be the empty string.
 *
 * @param path		The path
 * @param dir		(out param) The last path component of 'path'
 * @param dir_len	Length of the dir buffer
 *
 * @return		0 on success; -ENAMETOOLONG if bname_len wasn't long
 *			enough.
 */
extern int do_basename(char *bname, size_t bname_len, const char *fname);

#endif
