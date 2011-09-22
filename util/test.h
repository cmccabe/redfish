/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_UTIL_TEST_H
#define ONEFISH_UTIL_TEST_H

#include <stdio.h> /* for fprintf */

extern void die_unless(int t);
extern void die_if(int t);

#define EXPECT_ZERO(x) \
	do { \
		int __my_ret__ = x; \
		if (__my_ret__) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return __my_ret__; \
		} \
	} while (0);

#define EXPECT_NONZERO(x) \
	do { \
		int __my_ret__ = x; \
		if (__my_ret__ == 0) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return -1; \
		} \
	} while (0);

#define EXPECT_POSITIVE(x) \
	do { \
		int __my_ret__ = x; \
		if (__my_ret__ < 0) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return __my_ret__; \
		} \
	} while (0);

#define EXPECT_EQUAL(x, y) \
	do { \
		if (x != y) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return 1; \
		} \
	} while (0);

#define EXPECT_NOT_EQUAL(x, y) \
	do { \
		if (x == y) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return 1; \
		} \
	} while (0);

#define EXPECT_LT(x, y) \
	do { \
		if (x >= y) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return 1; \
		} \
	} while (0);

#endif
