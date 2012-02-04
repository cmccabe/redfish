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
#define PURE

#elif __GNUC__ /* GCC */
#define PACKED(D) D __attribute__((__packed__))
#define PACKED_ALIGNED(A, D) D __attribute__((__packed__, __aligned__(A)))
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#define restrict       __restrict__
#define POSSIBLY_UNUSED(x) x __attribute__((unused))
#define WARN_UNUSED_RES __attribute__((warn_unused_result))
#define PRINTF_FORMAT(x, y) __attribute__((format(printf, x, y)))
#define WEAK_SYMBOL(x) x __attribute__((weak))
#define PURE __attribute__((pure))

#else /* Unknown */
#error "sorry, I can't figure out what compiler you are using."
#endif

#endif
