/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_MACRO_H
#define REDFISH_UTIL_MACRO_H

#define TO_STR(x) #x

#define TO_STR2(x) TO_STR(x)

/** When X is true, force a compiler error.
 *
 * When X is true, we force a compiler error by declaring an array of negative
 * size.  This macro is useful for enforcing invariants at compile time.
 */
#define BUILD_BUG_ON(x) \
	extern char arr__##__LINE__ [0 - (!!(x))];

#endif
