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

#include "util/str_to_int.h"
#include "util/test.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int test_str_to_int(void)
{
	long long ll;
	int i;
	char err[512] = { 0 };

	err[0] = '\0';
	str_to_int("123", 10, &i, err, sizeof(err));
	EXPECT_ZERO(err[0]);
	EXPECT_EQUAL(i, 123);

	err[0] = '\0';
	str_to_int("0", 10, &i, err, sizeof(err));
	EXPECT_ZERO(err[0]);
	EXPECT_EQUAL(i, 0);

	err[0] = '\0';
	str_to_int("", 10, &i, err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	str_to_int("10b", 10, &i, err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	str_to_int("f", 16, &i, err, sizeof(err));
	EXPECT_ZERO(err[0]);
	EXPECT_EQUAL(i, 15);

	err[0] = '\0';
	str_to_int("8589934592", 10, &i, err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	str_to_int("2147483647", 10, &i, err, sizeof(err));
	EXPECT_ZERO(err[0]);
	EXPECT_EQUAL(i, 2147483647);

	err[0] = '\0';
	str_to_int("blah", 10, &i, err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	str_to_long_long("281474976710656", 10, &ll, err, sizeof(err));
	EXPECT_ZERO(err[0]);
	EXPECT_EQUAL(ll, 281474976710656LL);

	err[0] = '\0';
	str_to_long_long("0x1000000000000", 16, &ll, err, sizeof(err));
	EXPECT_ZERO(err[0]);
	EXPECT_EQUAL(ll, 281474976710656LL);

	err[0] = '\0';
	str_to_int("0755", 8, &i, err, sizeof(err));
	EXPECT_ZERO(err[0]);
	EXPECT_EQUAL(i, 493);

	return 0;
}

int main(void)
{
	EXPECT_ZERO(test_str_to_int());

	return EXIT_SUCCESS;
}
