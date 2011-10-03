/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
