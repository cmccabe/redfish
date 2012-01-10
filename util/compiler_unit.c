/*
 * Copyright 2011-2012 the RedFish authors
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
