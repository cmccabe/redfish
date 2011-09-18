/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/compiler.h"
#include "util/test.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

PACKED_ALIGNED(8,
struct pack_fun {
	uint32_t a;
	char b;
	char c;
	char d;
	char e;
	uint32_t f;
	uint64_t g;
}
);

PACKED(
struct total_unaligned {
	char a;
	uint32_t b;
	char c;
}
);

int main(void)
{
	struct pack_fun p;
	struct total_unaligned t;
	long int ptrdiff, ptrdiff2;

	/* Don't complain about this unused variable */
	POSSIBLY_UNUSED(int totally_unused_variable) = 0;

	ptrdiff = (char*)(&p.g) - (char*)(&p.a);
	die_unless(ptrdiff == 12);

	/* The compiler should not insert padding into the packed struct */
	ptrdiff2 = (char*)(&t.c) - (char*)(&t.a);
	die_unless(ptrdiff2 == 5);

	return EXIT_SUCCESS;
}
