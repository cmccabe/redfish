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
#include "util/compiler.h"
#include "util/string.h"

#include <unistd.h> /* for size_t */

#define JORM_TYCHECK_BAD_TY_MSG(name, aty, jty) \
	snappend(err, err_len, "WARNING: ignoring field \"%s%s\" " \
		 "because it has type %s, but it should have " \
		 "type %s.\n", acc, #name, json_ty_to_str(aty), \
		 json_ty_to_str(jty));

#define JORM_TYCHECK_IMPL(name, jty) { \
	struct json_object* ji = json_object_object_get(jo, #name); \
	if (ji) { \
		json_type aty = json_object_get_type(ji); \
		if (aty != jty) { \
			JORM_TYCHECK_BAD_TY_MSG(name, aty, jty) \
		} \
	} \
}

#define JORM_CONTAINER_BEGIN(ty) \
void JORM_TYCHECK_##ty(struct json_object *jo, \
		POSSIBLY_UNUSED(char* acc), POSSIBLY_UNUSED(size_t acc_len), \
		char *err, size_t err_len) {

#define JORM_INT(name) JORM_TYCHECK_IMPL(name, json_type_int)

#define JORM_DOUBLE(name) JORM_TYCHECK_IMPL(name, json_type_double)

#define JORM_STR(name) JORM_TYCHECK_IMPL(name, json_type_string)

#define JORM_NESTED(name, ty) { \
	struct json_object* ji = json_object_object_get(jo, #name); \
	if (ji) { \
		json_type aty = json_object_get_type(ji); \
		if (aty != json_type_object) { \
			JORM_TYCHECK_BAD_TY_MSG(name, aty, json_type_object) \
		} \
		else { \
			size_t cur_len = strlen(acc); \
			snappend(acc, acc_len, #name "/"); \
			JORM_TYCHECK_##ty(ji, acc, acc_len, err, err_len); \
			acc[cur_len] = '\0'; \
		} \
	} \
}

#define JORM_EMBEDDED(name, ty) \
	JORM_TYCHECK_##ty(jo, acc, acc_len, err, err_len);

#define JORM_BOOL(name) JORM_TYCHECK_IMPL(name, json_type_boolean)

#define JORM_ARRAY(name, ty) { \
struct json_object* ji = json_object_object_get(jo, #name); \
if (ji) { \
	json_type aty = json_object_get_type(ji); \
	if (aty != json_type_array) { \
		JORM_TYCHECK_BAD_TY_MSG(name, aty, json_type_array) \
	} \
	else { \
		int i, arr_len = json_object_array_length(ji); \
		for (i = 0; i < arr_len; ++i) { \
			struct json_object* ja = \
				json_object_array_get_idx(ji, i); \
			json_type jaty = json_object_get_type(ja); \
			if (jaty != json_type_object) { \
				char buf[512]; \
				snprintf(buf, sizeof(buf), #name "[%d]", i); \
				snappend(err, err_len, "WARNING: ignoring field " \
					 "\"%s" #name "[%d]\" because it has " \
					 "type %s, but it should have type " \
					 "%s.\n", acc, i, json_ty_to_str(aty), \
					 json_ty_to_str(jaty)); \
			} \
			else { \
				size_t cur_len = strlen(acc); \
				snappend(acc, acc_len, #name "[%d]/", i); \
				JORM_TYCHECK_##ty(ja, acc, acc_len, \
						  err, err_len); \
				acc[cur_len] = '\0'; \
			} \
		} \
	} \
} \
}

#define JORM_CONTAINER_END \
}

#define JORM_IGNORE(x)
