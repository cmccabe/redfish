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

#ifndef REDFISH_UTIL_TEST_H
#define REDFISH_UTIL_TEST_H

#include "util/error.h" /* for IS_ERR */

#include <stdio.h> /* for fprintf */

/** Abort unless t is true
 *
 * @param t		condition to check
 */
extern void die_unless(int t);

/** Abort if t is true
 *
 * @param t		condition to check
 */
extern void die_if(int t);

/** Create a zero-size file at ${file_name}
 *
 * @param fname		The file name
 *
 * @return		0 on success; error code otherwise
 */
extern int do_touch1(const char *fname);

/** Create a zero-size file at ${tempdir}/${file_name}
 *
 * @param dir		The dir
 * @param fname		The file name
 *
 * @return		0 on success; error code otherwise
 */
extern int do_touch2(const char *dir, const char *fname);

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
		if ((x) != (y)) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return 1; \
		} \
	} while (0);

#define EXPECT_NOT_EQUAL(x, y) \
	do { \
		if ((x) == (y)) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return 1; \
		} \
	} while (0);

#define EXPECT_LT(x, y) \
	do { \
		if ((x) >= (y)) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return 1; \
		} \
	} while (0);

#define EXPECT_GE(x, y) \
	do { \
		if ((x) < (y)) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return 1; \
		} \
	} while (0);

#define EXPECT_GT(x, y) \
	do { \
		if ((x) <= (y)) { \
			fprintf(stderr, "failed on line %d: %s\n",\
				__LINE__, #x); \
			return 1; \
		} \
	} while (0);

#define EXPECT_ERRPTR(p) \
	do { \
		if (!IS_ERR(p)) { \
			fprintf(stderr, "failed on line %d: failed to " \
				"find expected error\n", __LINE__); \
			return 1; \
		} \
	} while (0);

#define EXPECT_NOT_ERRPTR(p) \
	do { \
		if (IS_ERR(p)) { \
			fprintf(stderr, "failed on line %d: error %d\n",\
				__LINE__, PTR_ERR(p)); \
			return 1; \
		} \
	} while (0);

#endif
