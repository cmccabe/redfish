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

#ifndef REDFISH_UTIL_MACRO_H
#define REDFISH_UTIL_MACRO_H

#include <stddef.h> /* for offsetof */

#define TO_STR(x) #x

#define TO_STR2(x) TO_STR(x)

/** When X is true, force a compiler error.
 *
 * When X is true, we force a compiler error by declaring an array of negative
 * size.  This macro is useful for enforcing invariants at compile time.
 */
#define BUILD_BUG_ON(x) \
	extern char arr__##__LINE__ [0 - (!!(x))];

/** Given a pointer to a structure nested inside another structure,
 * return a pointer to the outer structure.
 *
 * This is possible because the compiler knows the offset at which the inner
 * structure is nested at.
 */
#define GET_OUTER(p, outer_ty, memb) \
	((outer_ty*)(((char*)p) - offsetof(outer_ty, memb)))

#endif
