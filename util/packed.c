/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/packed.h"

#include <endian.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
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

int pget_str(void *v, uint32_t *off, uint32_t len,
			char *out, size_t out_len)
{
	size_t o, rem;
	char *buf = (char*)v;
	int res;

	o = *off;
	if (o > len)
		return -EINVAL;
	rem = len - o;
	if (!memchr(buf + o, '\0', rem))
		return -EINVAL;
	res = snprintf(out, out_len, "%s", buf + o);
	if ((unsigned int)res > out_len)
		return -ENAMETOOLONG;
	*off = o + res + 1;
	return 0;
}

int pput_str(void *v, uint32_t *off, uint32_t len, char *str)
{
	size_t o, rem;
	int res;
	char *buf = (char*)v;

	o = *off;
	if (o > len)
		return -ENAMETOOLONG;
	rem = len - o;
	res = snprintf(buf + o, rem, "%s", str);
	if (((unsigned int)res) > rem)
		return -ENAMETOOLONG;
	*off = o + res + 1;
	return 0;
}
