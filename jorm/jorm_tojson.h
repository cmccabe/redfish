/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "jorm/jorm_const.h"
#include "jorm/json.h"

#include <json/json_object_private.h> /* need for struct json_object_iter */
#include <stdlib.h> /* for calloc */

#define JORM_CONTAINER_BEGIN(name) \
struct json_object *JORM_TOJSON_##name(struct name *me) { \
	struct json_object* jo = json_object_new_object(); \
	if (!jo) { \
		return NULL; \
	} \
	if (0) { \
		goto handle_oom; \
handle_oom: \
		json_object_put(jo); \
		return NULL; \
	} \

#define JORM_INT(name) \
if (me->name != JORM_INVAL_INT) { \
	struct json_object* ji = json_object_new_int(me->name); \
	if (!ji) { \
		goto handle_oom; \
	} \
	json_object_object_add(jo, #name, ji); \
}

#define JORM_DOUBLE(name) \
if (me->name != JORM_INVAL_DOUBLE) { \
	struct json_object* ji = json_object_new_double(me->name); \
	if (!ji) { \
		goto handle_oom; \
	} \
	json_object_object_add(jo, #name, ji); \
}

#define JORM_STR(name) \
if (me->name != JORM_INVAL_STR) { \
	struct json_object* ji = json_object_new_string(me->name); \
	if (!ji) { \
		goto handle_oom; \
	} \
	json_object_object_add(jo, #name, ji); \
}

#define JORM_NESTED(name, ty) \
if (me->name != JORM_INVAL_NESTED) { \
	struct json_object* ji = JORM_TOJSON_##ty(me->name); \
	if (!ji) { \
		goto handle_oom; \
	} \
	json_object_object_add(jo, #name, ji); \
}

#define JORM_EMBEDDED(name, ty) \
if (me->name != JORM_INVAL_EMBEDDED) { \
	struct json_object_iter iter; \
	struct json_object* ji = JORM_TOJSON_##ty(me->name); \
	if (!ji) { \
		goto handle_oom; \
	} \
	json_object_object_foreachC(ji, iter) { \
		json_object_get(iter.val); \
		json_object_object_add(jo, iter.key, iter.val); \
	} \
	json_object_put(ji); \
}

#define JORM_BOOL(name) \
if (me->name != JORM_INVAL_BOOL) { \
	struct json_object* ji = json_object_new_boolean(me->name); \
	if (!ji) { \
		goto handle_oom; \
	} \
	json_object_object_add(jo, #name, ji); \
}

#define JORM_ARRAY(name, ty) \
if (me->name != JORM_INVAL_ARRAY) { \
	int i; \
	struct json_object* ji = json_object_new_array(); \
	if (!ji) { \
		goto handle_oom; \
	} \
	json_object_object_add(jo, #name, ji); \
	for (i = 0; me->name[i]; ++i) { \
		struct json_object* ja = JORM_TOJSON_##ty(me->name[i]); \
		if (!ja) { \
			goto handle_oom; \
		} \
		if (json_object_array_add(ji, ja) != 0) { \
			goto handle_oom; \
		} \
	} \
}

#define JORM_CONTAINER_END \
	return jo; \
}

#define JORM_IGNORE(x)
