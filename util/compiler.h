/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_COMPILER_H
#define REDFISH_UTIL_COMPILER_H

/* Some compiler-specific stuff.
 *
 * Note: PACKED_ALIGNED will usually generate better code than PACKED, so use
 * it if you can.
 */

#ifdef __MSVC__ /* MS Visual Studio */
// TODO: actually test this to see if it compiles under Windows
#define PACKED(D) __pragma( pack(push, 1) ) D __pragma( pack(pop) )
#define PACKED_ALIGNED(A, D) __pragma( pack(push, A) ) D __pragma( pack(pop) )
#define likely(x)
#define unlikely(x)
#define restrict __restrict
#define POSSIBLY_UNUSED(x)
#define WARN_UNUSED_RES
#define PRINTF_FORMAT(x, y)

#elif __GNUC__ /* GCC */
#define PACKED(D) D __attribute__((__packed__))
#define PACKED_ALIGNED(A, D) D __attribute__((__packed__, __aligned__(A)))
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#define restrict       __restrict__
#define POSSIBLY_UNUSED(x) x __attribute__((unused))
#define WARN_UNUSED_RES __attribute__((warn_unused_result))
#define PRINTF_FORMAT(x, y) __attribute__((format(printf, x, y)))

#else /* Unknown */
#error "sorry, I can't figure out what compiler you are using."
#endif

#endif
