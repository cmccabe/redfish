/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_PLATFORM_THREAD_ID_DOT_H
#define REDFISH_UTIL_PLATFORM_THREAD_ID_DOT_H

#include <stdint.h> /* for uint32_t */

/** Create a unique thread ID.
 *
 * Only call this once per thread.  It _may_ return a new value each time you
 * call it.
 *
 * @return		a unique thread id
 */
uint32_t create_unique_thread_id(void);

#endif
