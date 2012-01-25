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
	EXPECT_NOT_EQ(outer, NULL);

	outer->i = 123;
	outer->my_inner.j = 456;

	my_outer = GET_OUTER(&outer->my_inner, struct outer, my_inner);
	EXPECT_EQ(outer, my_outer);

	free(outer);
	return EXIT_SUCCESS;
}
