/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "jorm/jorm_const.h"

#include <stdlib.h> /* for free */

#define JORM_CONTAINER_BEGIN(name) \
void JORM_FREE_##name(struct name *jorm) { \

#define JORM_INT(name) \
	jorm->name = JORM_INVAL_INT;

#define JORM_DOUBLE(name) \
	jorm->name = JORM_INVAL_DOUBLE;

#define JORM_STR(name) \
	if (jorm->name != JORM_INVAL_STR) { \
		free(jorm->name); \
		jorm->name = JORM_INVAL_STR; \
	}

#define JORM_NESTED(name, ty) \
	if (jorm->name != JORM_INVAL_NESTED) { \
		JORM_FREE_##ty(jorm->name); \
		jorm->name = JORM_INVAL_NESTED; \
	}

#define JORM_EMBEDDED(name, ty) \
	if (jorm->name != JORM_INVAL_EMBEDDED) { \
		JORM_FREE_##ty(jorm->name); \
		jorm->name = JORM_INVAL_EMBEDDED; \
	}

#define JORM_BOOL(name) \
	jorm->name = JORM_INVAL_BOOL;

#define JORM_ARRAY(name, ty) \
	JORM_ARRAY_FREE_##ty(&jorm->name);

#define JORM_CONTAINER_END \
	free(jorm); \
}

#define JORM_IGNORE(x)
