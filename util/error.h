/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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

#define FORCE_POSITIVE(e) ((e < 0) ? -e : e)

#define FORCE_NEGATIVE(e) ((e > 0) ? -e : e)

#define REDFISH_TEMP_ERROR 6000

#endif
