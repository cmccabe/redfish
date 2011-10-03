/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/config.h"
#include "util/test.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

static int test_parse_file_size(void)
{
	uint64_t res;
	char out[128];
	res = parse_file_size("123", out, sizeof(out));
	if (res != 123llu) {
		return -EINVAL;
	}

	res = parse_file_size("1111111111111", out, sizeof(out));
	if (res != 1111111111111llu) {
		return -EINVAL;
	}

	res = parse_file_size("", out, sizeof(out));
	if (res != 0) {
		return -EINVAL;
	}

	res = parse_file_size("2KB", out, sizeof(out));
	if (res != 2048llu) {
		return -EINVAL;
	}

	res = parse_file_size("5MB", out, sizeof(out));
	if (res != 5242880llu) {
		return -EINVAL;
	}

	res = parse_file_size("16GB", out, sizeof(out));
	if (res != 17179869184llu) {
		return -EINVAL;
	}
	return 0;
}

int main(void)
{
	int ret;
	ret = test_parse_file_size();
	if (ret) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
