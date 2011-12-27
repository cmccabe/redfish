/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
