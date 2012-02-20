/*
 * Copyright 2012 the Redfish authors
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

#include "util/circ_compare.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int circ_compare16(uint16_t a, uint16_t b)
{
	uint16_t c;

	c = b;
	c -= a;
	if (c > 0x8000)
		return 1;
	else if (c == 0)
		return 0;
	else
		return -1;
}
