/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_UTIL_PLATFORM_READDIR_DOT_H
#define ONEFISH_UTIL_PLATFORM_READDIR_DOT_H

struct dirent;
struct onefish_dirp;

/** Open a directory for reading. Similar to opendir(3)
 *
 * @param name		Name of directory to open
 * @param dp		(out param) on success; a pointer to the opened
 *			directory
 *
 * @return		0 on success; error code otherwise
 */
int  do_opendir(const char *name, struct onefish_dirp** dp);

/** Read another entry from a directory.
 *
 * This is similar to readdir, but guaranteed to be thread-safe on every
 * platform.
 *
 * @param dp		directory to read
 *
 * @return		the next directory entry, or NULL if there are no more.
 */
struct dirent *do_readdir(struct onefish_dirp *dp);

/** Close a directory and free the associated resources
 *
 * @param dp		directory to close
 */
void do_closedir(struct onefish_dirp *dp);

#endif
