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

#include "util/platform/thread_id.h"

#include <linux/unistd.h>
#include <sys/syscall.h>
#include <unistd.h>

#define INVALID_TID 0xffffffffU

/** The current thread ID.  We're in the Linux-specific code section, so we can
 * use ELF TLS here */
static __thread uint32_t g_tid = INVALID_TID;

uint32_t get_tid(void)
{
	if (g_tid == INVALID_TID) {
		/* Use the super secret gettid() call to get the real kernel
		 * thread ID.  We can do this because we know that on Linux,
		 * people are using a 1-to-1 pthreads package.
		 */
		g_tid = syscall(__NR_gettid);
	}
	return g_tid;
}
