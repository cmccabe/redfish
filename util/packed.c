/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/packed.h"

#include <endian.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

uint8_t pbe8_to_h(void *v)
{
	uint8_t u;
	memcpy(&u, v, sizeof(uint8_t));
	return u;
}

void ph_to_be8(void *v, uint8_t u)
{
	memcpy(v, &u, sizeof(uint8_t));
}

uint16_t pbe16_to_h(void *v)
{
	uint16_t u;
	memcpy(&u, v, sizeof(uint16_t));
	return be16toh(u);
}

void ph_to_be16(void *v, uint16_t u)
{
	u = htobe16(u);
	memcpy(v, &u, sizeof(uint16_t));
}

uint32_t pbe32_to_h(void *v)
{
	uint32_t u;
	memcpy(&u, v, sizeof(uint32_t));
	return be32toh(u);
}

void ph_to_be32(void *v, uint32_t u)
{
	u = htobe32(u);
	memcpy(v, &u, sizeof(uint32_t));
}

uint64_t pbe64_to_h(void *v)
{
	uint64_t u;
	memcpy(&u, v, sizeof(uint64_t));
	return be64toh(u);
}

void ph_to_be64(void *v, uint64_t u)
{
	u = htobe64(u);
	memcpy(v, &u, sizeof(uint64_t));
}

