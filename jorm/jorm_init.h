/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "jorm/jorm_const.h"

#include <stdlib.h> /* for malloc */

#define JORM_CONTAINER_BEGIN(ty) \
struct ty* JORM_INIT_##ty(void) { \
	/* It's convenient to use calloc here so that JORM_IGNORE fields are \
	 * initialized to all 0. \
	 */ \
	struct ty* me = calloc(1, sizeof(struct ty)); \
	if (!me) \
		return NULL; \

#define JORM_INT(name) \
	me->name = JORM_INVAL_INT;

#define JORM_DOUBLE(name) \
	me->name = JORM_INVAL_DOUBLE;

#define JORM_STR(name) \
	me->name = JORM_INVAL_STR;

#define JORM_NESTED(name, ty) \
	me->name = NULL;

#define JORM_EMBEDDED(name, ty) \
	me->name = NULL;

#define JORM_BOOL(name) \
	me->name = JORM_INVAL_BOOL;

#define JORM_ARRAY(name, ty) \
	me->name = JORM_INVAL_ARRAY;

#define JORM_CONTAINER_END \
	return me; \
}

#define JORM_IGNORE(x)
