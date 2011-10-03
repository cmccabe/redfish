/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_SLEEP_DOT_H
#define REDFISH_UTIL_SLEEP_DOT_H

/** Sleep for a given number of milliseconds.
 * Unlike sleep(3), this is guaranteed not to use signals.
 *
 * @param milli		Number of milliseconds to sleep
 */
void do_msleep(int milli);

#endif
