/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "jorm/jorm_const.h"
#include "jorm/json.h"

#include <stdlib.h> /* for calloc */
#include <string.h> /* for strdup */

#define JORM_CONTAINER_BEGIN(name) \
struct name *JORM_FROMJSON_##name(struct json_object *jo) { \
	struct name *out = calloc(1, sizeof(struct name)); \
	if (!out) { \
		return NULL; \
	} \
	if (0) { \
		goto handle_oom; \
handle_oom: \
		JORM_FREE_##name(out); \
		return NULL; \
	}
#define JORM_INT(name) { \
	struct json_object* ji = json_object_object_get(jo, #name); \
	if (ji && (json_object_get_type(ji) == json_type_int)) { \
		out->name = json_object_get_int(ji); \
	} \
	else { \
		out->name = JORM_INVAL_INT; \
	} \
}
#define JORM_DOUBLE(name) { \
	struct json_object* ji = json_object_object_get(jo, #name); \
	if (ji && (json_object_get_type(ji) == json_type_double)) { \
		out->name = json_object_get_double(ji); \
	} \
	else { \
		out->name = JORM_INVAL_DOUBLE; \
	} \
}
#define JORM_STR(name) { \
	struct json_object* ji = json_object_object_get(jo, #name); \
	if (ji && (json_object_get_type(ji) == json_type_string)) { \
		out->name = strdup(json_object_get_string(ji)); \
		if (!out->name) { \
			goto handle_oom; \
		} \
	} \
	else { \
		out->name = JORM_INVAL_STR; \
	} \
}
#define JORM_NESTED(name, ty) { \
	struct json_object* ji = json_object_object_get(jo, #name); \
	if (ji && (json_object_get_type(ji) == json_type_object)) { \
		out->name = JORM_FROMJSON_##ty(ji); \
		if (!out->name) { \
			goto handle_oom; \
		} \
	} \
	else { \
		out->name = JORM_INVAL_NESTED; \
	} \
}
#define JORM_EMBEDDED(name, ty) { \
	out->name = JORM_FROMJSON_##ty(jo); \
	if (!out->name) { \
		goto handle_oom; \
	} \
}
#define JORM_BOOL(name) { \
	struct json_object* ji = json_object_object_get(jo, #name); \
	if (ji && (json_object_get_type(ji) == json_type_boolean)) { \
		out->name = json_object_get_boolean(ji); \
	} \
	else { \
		out->name = JORM_INVAL_BOOL; \
	} \
}
#define JORM_ARRAY(name, ty) { \
	struct json_object* ji = json_object_object_get(jo, #name); \
	if (ji && (json_object_get_type(ji) == json_type_array)) { \
		int i, arr_len = json_object_array_length(ji); \
		out->name = calloc(1, (arr_len + 1) * sizeof(struct ty*)); \
		if (!out->name) { \
			goto handle_oom; \
		} \
		for (i = 0; i < arr_len; ++i) { \
			struct json_object* ja = \
				json_object_array_get_idx(ji, i); \
			if (!ja) { \
				goto handle_oom; \
			} \
			if (json_object_get_type(ja) != json_type_object) { \
				continue; \
			} \
			out->name[i] = JORM_FROMJSON_##ty(ja); \
			if (!out->name[i]) { \
				goto handle_oom; \
			} \
		} \
	} \
	else { \
		out->name = JORM_INVAL_ARRAY; \
	} \
}
#define JORM_CONTAINER_END \
	return out; \
}
#define JORM_IGNORE(x)
