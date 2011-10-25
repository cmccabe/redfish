/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "msg/msg.h"

#include <endian.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void *calloc_msg(uint32_t ty, uint32_t len)
{
	struct msg *m = calloc(1, sizeof(struct msg) + len);
	if (!m)
		return NULL;
	m->len = htobe32(len);
	m->ty = htobe16(ty);
	return m;
}
