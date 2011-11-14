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

#include <errno.h>
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

static int test_fixed_field_functions(void)
{
	struct pack_fun s;
	memset(&s, 0, sizeof(s));
	EXPECT_EQUAL(unpack_from_8(&s.a), 0);
	EXPECT_EQUAL(unpack_from_be16(&s.b), 0);
	EXPECT_EQUAL(unpack_from_be32(&s.c), 0);
	EXPECT_EQUAL(unpack_from_be64(&s.d), 0);

	pack_to_8(&s.a, 123);
	EXPECT_EQUAL(unpack_from_8(&s.a), 123);
	pack_to_be16(&s.b, 456);
	EXPECT_EQUAL(unpack_from_be16(&s.b), 456);
	pack_to_be32(&s.c, 0xdeadbeef);
	EXPECT_EQUAL(unpack_from_be32(&s.c), 0xdeadbeef);
	pack_to_be64(&s.d, 0xdeadbeefbaddcafell);
	EXPECT_EQUAL(unpack_from_be64(&s.d), 0xdeadbeefbaddcafell);

	return 0;
}

static int test_packed_string_functions(void)
{
	char buf[128] = { 0 };
	char buf2[128] = { 0 };
	uint32_t off, off2;

	off = 0;
	EXPECT_ZERO(pack_str(buf, &off, sizeof(buf), "swubu"));

	off2 = 0;
	EXPECT_ZERO(unpack_str(buf, &off2, sizeof(buf), buf2, sizeof(buf2)));
	EXPECT_ZERO(strcmp("swubu", buf2));

	EXPECT_ZERO(pack_str(buf, &off, sizeof(buf), "zub-zub"));
	EXPECT_ZERO(pack_str(buf, &off, sizeof(buf), "glor-duk"));
	EXPECT_ZERO(pack_str(buf, &off, sizeof(buf), "loktar"));

	off2 = 0;
	unpack_str(buf, &off2, off, buf2, sizeof(buf2));
	EXPECT_ZERO(strcmp("swubu", buf2));
	EXPECT_ZERO(unpack_str(buf, &off2, off, buf2, sizeof(buf2)));
	EXPECT_ZERO(strcmp("zub-zub", buf2));
	EXPECT_ZERO(unpack_str(buf, &off2, off, buf2, sizeof(buf2)));
	EXPECT_ZERO(strcmp("glor-duk", buf2));
	EXPECT_ZERO(unpack_str(buf, &off2, off, buf2, sizeof(buf2)));
	EXPECT_ZERO(strcmp("loktar", buf2));
	EXPECT_EQUAL(-EINVAL, unpack_str(buf, &off2, off, buf2, sizeof(buf2)));

	off = 0;
	memset(buf, 0, sizeof(buf));
	EXPECT_EQUAL(-ENAMETOOLONG, pack_str(buf, &off, 4, "toolongstring"));

	return 0;
}

int main(void)
{
	EXPECT_ZERO(test_fixed_field_functions());
	EXPECT_ZERO(test_packed_string_functions());

	return EXIT_SUCCESS;
}
