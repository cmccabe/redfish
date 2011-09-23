/*
 * The OneFish distributed filesystem
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

int main(void)
{
	return EXIT_SUCCESS;
}
