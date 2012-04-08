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
#include "jorm/json.h"

#include <stdlib.h> /* for calloc */
#include <string.h> /* for strdup */

#define JORM_CONTAINER_BEGIN(ty) \
struct ty *JORM_FROMJSON_##ty(struct json_object *jo) { \
	struct ty *out = JORM_INIT_##ty(); \
	if (!out) { \
		return NULL; \
	} \
	if (0) { \
		goto handle_oom; \
handle_oom: \
		JORM_FREE_##ty(out); \
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
#define JORM_SARRAY(name) { \
	struct json_object* ji = json_object_object_get(jo, #name); \
	if (ji && (json_object_get_type(ji) == json_type_array)) { \
		int i, arr_len = json_object_array_length(ji); \
		out->name = calloc(1, (arr_len + 1) * sizeof(char *)); \
		if (!out->name) { \
			goto handle_oom; \
		} \
		for (i = 0; i < arr_len; ++i) { \
			struct json_object* ja = \
				json_object_array_get_idx(ji, i); \
			if (!ja) { \
				goto handle_oom; \
			} \
			if (json_object_get_type(ja) != json_type_string) { \
				continue; \
			} \
			out->name[i] = strdup(json_object_get_string(ja)); \
			if (!out->name[i]) { \
				goto handle_oom; \
			} \
		} \
	} \
	else { \
		out->name = JORM_INVAL_ARRAY; \
	} \
}
#define JORM_OARRAY(name, ty) { \
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
