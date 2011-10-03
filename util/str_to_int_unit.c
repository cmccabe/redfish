/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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

	return 0;
}

int main(void)
{
	EXPECT_ZERO(test_str_to_int());

	return EXIT_SUCCESS;
}
