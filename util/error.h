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

#ifndef REDFISH_UTIL_ERROR_H
#define REDFISH_UTIL_ERROR_H

#include "util/compiler.h"

#include <stdint.h>

#define REDFISH_MAX_ERR 1024

/*
 * Scheme for packing error codes into pointers.
 */
static inline void *ERR_PTR(int error) {
  return (void*)(uintptr_t)error;
}

static inline int PTR_ERR(const void *ptr) {
  return (int)(uintptr_t)ptr;
}

static inline int IS_ERR(const void *ptr) {
  return unlikely((uintptr_t)ptr < (uintptr_t)REDFISH_MAX_ERR);
}

#define RETRY_ON_EINTR(ret, expr) do { \
  ret = expr; \
} while ((ret == -1) && (errno == EINTR));

#define RETRY_ON_EINTR_GET_ERRNO(ret, expr) do { \
ret = expr; \
if (!ret) \
	break; \
ret = -errno; \
} while (ret == -EINTR);

#define FORCE_POSITIVE(e) ((e < 0) ? -e : e)

#define FORCE_NEGATIVE(e) ((e > 0) ? -e : e)

#define REDFISH_TEMP_ERROR 6000

#endif
