/*
 * Copyright 2011-2012 the Redfish authors
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

#define JORM_OARRAY(name, ty) \
	JORM_OARRAY_FREE_##ty(&jorm->name);

#define JORM_CONTAINER_END \
	free(jorm); \
}

#define JORM_IGNORE(x)
