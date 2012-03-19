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
	char err[512];

	err[0] = '\0';
	EXPECT_EQ(123, str_to_int("123", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(-123, str_to_int("-123", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(123, str_to_int("123    ", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(123, str_to_int("    123", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(123, str_to_int("    123    ", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(-123, str_to_int("    -123    ", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(0, str_to_int("0", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	str_to_int("", err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	str_to_int("10b", err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	str_to_int("f", err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	str_to_int("8589934592", err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(2147483647, str_to_int("2147483647", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(281474976710656LL,
		str_to_u64("281474976710656", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(0x1000000000000LLU,
		str_to_u64("0x1000000000000", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	str_to_u64("-123", err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	str_to_u64("-0x123", err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(-0x123, str_to_s64("-0x123", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(0755, str_to_oct("0755", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(0755, str_to_oct("755", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	EXPECT_EQ(0755, str_to_oct("   755", err, sizeof(err)));
	EXPECT_ZERO(err[0]);

	err[0] = '\0';
	str_to_oct("955", err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	err[0] = '\0';
	str_to_oct("   955", err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	return 0;
}

int main(void)
{
	EXPECT_ZERO(test_str_to_int());

	return EXIT_SUCCESS;
}
