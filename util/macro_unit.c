/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/macro.h"
#include "util/test.h"

#include <stdint.h>
#include <stdlib.h>

BUILD_BUG_ON(sizeof(uint32_t) < sizeof(uint16_t))

struct inner {
	int j;
};

struct outer {
	int i;
	struct inner my_inner;
};

int main(void)
{
	struct outer *outer, *my_outer;

	outer = calloc(1, sizeof(struct outer));
	EXPECT_NOT_EQUAL(outer, NULL);

	outer->i = 123;
	outer->my_inner.j = 456;

	my_outer = GET_OUTER(&outer->my_inner, struct outer, my_inner);
	EXPECT_EQUAL(outer, my_outer);

	free(outer);
	return EXIT_SUCCESS;
}
