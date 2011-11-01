/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#ifndef REDFISH_UTIL_BITFIELD_DOT_H
#define REDFISH_UTIL_BITFIELD_DOT_H

#include <stdint.h> /* for uint8_t, etc. */
#include <string.h> /* for memset, etc. */

#define BITFIELD_DECL(name, size) uint8_t name[(size + 7)/ 8];

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

#define BITFIELD_CLEAR(name, idx) do { \
	uint8_t *name8 = name; \
	int floor = idx / 8; \
	int rem = idx - (floor * 8); \
	name8[floor] &= ~(1 << rem); \
} while(0);

#define BITFIELD_TEST(name, idx) \
	((name[idx / 8] >> (idx % 8)) & 0x1)

#endif
