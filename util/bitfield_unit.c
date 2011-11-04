/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/bitfield.h"
#include "util/macro.h"
#include "util/test.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BF_SIZE 64

static int bitfield_test1(void)
{
	int i;

	BITFIELD_DECL(foo, BF_SIZE);
	BITFIELD_DECL(bar, BF_SIZE);
	EXPECT_NONZERO(sizeof(foo) == 8);
	BITFIELD_ZERO(foo);
	for (i = 0; i < BF_SIZE; ++i) {
		EXPECT_ZERO(BITFIELD_TEST(foo, i));
	}
	BITFIELD_SET(foo, 14);
	EXPECT_NONZERO(BITFIELD_TEST(foo, 14));
	BITFIELD_SET(foo, 32);
	EXPECT_NONZERO(BITFIELD_TEST(foo, 32));
	BITFIELD_SET(foo, 1);
	EXPECT_NONZERO(BITFIELD_TEST(foo, 1));
	BITFIELD_FILL(foo);
	for (i = 0; i < BF_SIZE; ++i) {
		EXPECT_NONZERO(BITFIELD_TEST(foo, i));
	}
	BITFIELD_COPY(bar, foo);
	for (i = 0; i < BF_SIZE; ++i) {
		EXPECT_NONZERO(BITFIELD_TEST(foo, i));
	}
	return 0;
}

#undef BF_SIZE
#define BF_SIZE 129

struct foo {
	BITFIELD_DECL(bf, BF_SIZE);
};

static int bitfield_test2(void)
{
	int i;
	struct foo f;

	BITFIELD_ZERO(f.bf);
	for (i = 0; i < BF_SIZE; ++i) {
		EXPECT_ZERO(BITFIELD_TEST(f.bf, i));
	}
	EXPECT_NONZERO(sizeof(f.bf) == 17);
	BITFIELD_FILL(f.bf);
	for (i = 0; i < BF_SIZE; ++i) {
		EXPECT_NONZERO(BITFIELD_TEST(f.bf, i));
	}
	for (i = 0; i < BF_SIZE; ++i) {
		BITFIELD_CLEAR(f.bf, i);
	}
	for (i = 0; i < BF_SIZE; ++i) {
		EXPECT_ZERO(BITFIELD_TEST(f.bf, i));
	}
	for (i = 0; i < BF_SIZE; ++i) {
		BITFIELD_SET(f.bf, i);
	}
	for (i = 0; i < BF_SIZE; ++i) {
		EXPECT_NONZERO(BITFIELD_TEST(f.bf, i));
	}
	return 0;
}

int main(void)
{
	EXPECT_ZERO(bitfield_test1());
	EXPECT_ZERO(bitfield_test2());

	return EXIT_SUCCESS;
}
