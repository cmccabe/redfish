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

#include "util/packed.h"

#include <endian.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint8_t unpack_from_8(const void *v)
{
	uint8_t u;
	memcpy(&u, v, sizeof(uint8_t));
	return u;
}

void pack_to_8(void *v, uint8_t u)
{
	memcpy(v, &u, sizeof(uint8_t));
}

/**************** pack to big-endian byte order ********************/
uint16_t unpack_from_be16(const void *v)
{
	uint16_t u;
	memcpy(&u, v, sizeof(uint16_t));
	return be16toh(u);
}

void pack_to_be16(void *v, uint16_t u)
{
	u = htobe16(u);
	memcpy(v, &u, sizeof(uint16_t));
}

uint32_t unpack_from_be32(const void *v)
{
	uint32_t u;
	memcpy(&u, v, sizeof(uint32_t));
	return be32toh(u);
}

void pack_to_be32(void *v, uint32_t u)
{
	u = htobe32(u);
	memcpy(v, &u, sizeof(uint32_t));
}

uint64_t unpack_from_be64(const void *v)
{
	uint64_t u;
	memcpy(&u, v, sizeof(uint64_t));
	return be64toh(u);
}

void pack_to_be64(void *v, uint64_t u)
{
	u = htobe64(u);
	memcpy(v, &u, sizeof(uint64_t));
}

/**************** pack to host byte order ********************/
uint16_t unpack_from_h16(const void *v)
{
	uint16_t u;
	memcpy(&u, v, sizeof(uint16_t));
	return u;
}

void pack_to_h16(void *v, uint16_t u)
{
	memcpy(v, &u, sizeof(uint16_t));
}

uint32_t unpack_from_h32(const void *v)
{
	uint32_t u;
	memcpy(&u, v, sizeof(uint32_t));
	return u;
}

void pack_to_h32(void *v, uint32_t u)
{
	memcpy(v, &u, sizeof(uint32_t));
}

uint64_t unpack_from_h64(const void *v)
{
	uint64_t u;
	memcpy(&u, v, sizeof(uint64_t));
	return u;
}

void pack_to_h64(void *v, uint64_t u)
{
	memcpy(v, &u, sizeof(uint64_t));
}

void* unpack_from_hptr(const void *v)
{
	void *ptr;
	memcpy(&ptr, v, sizeof(void*));
	return ptr;
}

void pack_to_hptr(void *v, void *ptr)
{
	memcpy(v, &ptr, sizeof(void*));
}

/**************** pack to variable-sized data ********************/
int unpack_str(const void *v, uint32_t *off, uint32_t len,
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

int pack_str(void *v, uint32_t *off, uint32_t len, const char *str)
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

int repack_str(void *ov, uint32_t *ooff, uint32_t olen,
		const void *iv, uint32_t *ioff, uint32_t ilen)
{
	size_t x, y, orem, irem;
	int res;
	char *o = (char*)ov;
	char *i = (char*)iv;

	x = *ooff;
	if (x > olen)
		return -ENAMETOOLONG;
	orem = olen - x;
	y = *ioff;
	if (y > ilen)
		return -EINVAL;
	irem = ilen - y;
	if (!memchr(i + y, '\0', irem))
		return -EINVAL;
	res = snprintf(o + x, orem, "%s", i + y);
	if ((unsigned int)res > orem)
		return -ENAMETOOLONG;
	*ooff = x + res + 1;
	*ioff = y + res + 1;
	return 0;
}
