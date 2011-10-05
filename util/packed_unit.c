/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/compiler.h"
#include "util/packed.h"
#include "util/test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PACKED_ALIGNED(8,
struct pack_fun {
	uint8_t a;
	uint16_t b;
	uint32_t c;
	uint64_t d;
}
);

int main(void)
{
	struct pack_fun s;
	memset(&s, 0, sizeof(s));
	EXPECT_EQUAL(pbe8_to_h(&s.a), 0);
	EXPECT_EQUAL(pbe16_to_h(&s.b), 0);
	EXPECT_EQUAL(pbe32_to_h(&s.c), 0);
	EXPECT_EQUAL(pbe64_to_h(&s.d), 0);

	ph_to_be8(&s.a, 123);
	EXPECT_EQUAL(pbe8_to_h(&s.a), 123);
	ph_to_be16(&s.b, 456);
	EXPECT_EQUAL(pbe16_to_h(&s.b), 456);
	ph_to_be32(&s.c, 0xdeadbeef);
	EXPECT_EQUAL(pbe32_to_h(&s.c), 0xdeadbeef);
	ph_to_be64(&s.d, 0xdeadbeefbaddcafell);
	EXPECT_EQUAL(pbe64_to_h(&s.d), 0xdeadbeefbaddcafell);

	return EXIT_SUCCESS;
}
