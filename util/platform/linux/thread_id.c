/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/platform/thread_id.h"

#include <linux/unistd.h>
#include <sys/syscall.h>
#include <unistd.h>

uint32_t create_unique_thread_id(void)
{
	/* Use the super secret gettid() call to get the real kernel
	 * thread ID */
	return syscall(__NR_gettid);
}
