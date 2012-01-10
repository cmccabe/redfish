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

#ifndef REDFISH_UTIL_BITFIELD_DOT_H
#define REDFISH_UTIL_BITFIELD_DOT_H

#include <stdint.h> /* for uint8_t, etc. */
#include <string.h> /* for memset, etc. */

#define BITFIELD_DECL(name, size) uint8_t name[(size + 7)/ 8]

#define BITFIELD_ZERO(name) do { \
	uint8_t *name8 = name; \
	memset(name8, 0, sizeof(name)); \
} while(0);

#define BITFIELD_FILL(name) do { \
	uint8_t *name8 = name; \
	memset(name8, ~0, sizeof(name)); \
} while(0);

#define BITFIELD_SET(name, idx) do { \
	uint8_t *name8 = name; \
	int floor = idx / 8; \
	int rem = idx - (floor * 8); \
	name8[floor] |= (1 << rem); \
} while(0);

#define BITFIELD_COPY(dst, src) do { \
	uint8_t *src8 = src; \
	memcpy(dst, src8, sizeof(src)); \
} while(0);

#define BITFIELD_CLEAR(name, idx) do { \
	uint8_t *name8 = name; \
	int floor = idx / 8; \
	int rem = idx - (floor * 8); \
	name8[floor] &= ~(1 << rem); \
} while(0);

#define BITFIELD_TEST(name, idx) \
	((name[idx / 8] >> (idx % 8)) & 0x1)

#endif
