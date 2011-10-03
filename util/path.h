/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
