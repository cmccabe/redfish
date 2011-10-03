/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/msleep.h"

#include <poll.h>
#include <stdlib.h>

void do_msleep(int milli)
{
	poll(NULL, 0, milli);
}
