/*
 * Copyright 2011-2012 the RedFish authors
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

uint32_t create_unique_thread_id(void)
{
	/* Use the super secret gettid() call to get the real kernel
	 * thread ID */
	return syscall(__NR_gettid);
}
