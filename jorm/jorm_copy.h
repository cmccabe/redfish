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

#include <errno.h> /* for ENOMEM */
#include <string.h> /* for strdup */

/* Note: If JORM_COPY exits with an out-of-memory condition, parts of the source
 * might still have been copied to the destination.
 */
#define JORM_CONTAINER_BEGIN(name) \
int JORM_COPY_##name(struct name *src, struct name *dst) { \
	if (!src) \
		return 0; \

#define JORM_INT(name) \
	if (src->name != JORM_INVAL_INT) \
		dst->name = src->name;

#define JORM_DOUBLE(name) \
	if (src->name != JORM_INVAL_DOUBLE) \
		dst->name = src->name;

#define JORM_STR(name) \
	if (src->name != JORM_INVAL_STR) { \
		free(dst->name); \
		dst->name = strdup(src->name); \
		if (!dst->name) \
			return -ENOMEM; \
	}

#define JORM_NESTED(name, ty) \
	if (src->name != JORM_INVAL_NESTED) { \
		int ret; \
		if (dst->name == JORM_INVAL_NESTED) { \
			dst->name = JORM_INIT_##ty(); \
			if (!dst->name) \
				return -ENOMEM; \
		} \
		ret = JORM_COPY_##ty(src->name, dst->name); \
		if (ret) \
			return ret; \
	}

#define JORM_EMBEDDED(name, ty) \
	if (src->name != JORM_INVAL_EMBEDDED) { \
		int ret; \
		if (dst->name == JORM_INVAL_EMBEDDED) { \
			dst->name = JORM_INIT_##ty(); \
			if (!dst->name) \
				return -ENOMEM; \
		} \
		ret = JORM_COPY_##ty(src->name, dst->name); \
		if (ret) \
			return ret; \
	}

#define JORM_BOOL(name) \
	if (src->name != JORM_INVAL_BOOL) \
		dst->name = src->name;

#define JORM_OARRAY(name, ty) \
	if (src->name != JORM_INVAL_ARRAY) { \
		struct ty **arr; \
		int i, slen, dlen; \
		for (slen = 0; src->name[slen]; ++slen) { \
			; \
		} \
		if (dst->name == JORM_INVAL_ARRAY) \
			dlen = 0; \
		else { \
			for (dlen = 0; dst->name[dlen]; ++dlen) { \
				; \
			} \
		} \
		if (slen > dlen) { \
			arr = realloc(dst->name, (slen + 1) * sizeof(struct ty*)); \
			if (!arr) \
				return -ENOMEM; \
			memset(arr + dlen, 0, (1 + slen - dlen) * sizeof(struct ty*)); \
			dst->name = arr; \
		} \
		for (i = 0; i < slen; ++i) { \
			int ret; \
			if (dst->name[i] == NULL) { \
				dst->name[i] = JORM_INIT_##ty(); \
				if (!dst->name[i]) \
					return -ENOMEM; \
			} \
			ret = JORM_COPY_##ty(src->name[i], dst->name[i]); \
			if (ret) \
				return ret; \
		} \
	}

#define JORM_SARRAY(name) \
	if (src->name != JORM_INVAL_ARRAY) { \
		char **arr; \
		int i, slen, dlen; \
		for (slen = 0; src->name[slen]; ++slen) { \
			; \
		} \
		if (dst->name == JORM_INVAL_ARRAY) \
			dlen = 0; \
		else { \
			for (dlen = 0; dst->name[dlen]; ++dlen) { \
				; \
			} \
		} \
		if (slen > dlen) { \
			arr = realloc(dst->name, (slen + 1) * sizeof(char *)); \
			if (!arr) \
				return -ENOMEM; \
			memset(arr + dlen, 0, (1 + slen - dlen) * sizeof(char *)); \
			dst->name = arr; \
		} \
		for (i = 0; i < slen; ++i) { \
			if (dst->name[i]) \
				free(dst->name[i]); \
			dst->name[i] = strdup(src->name[i]); \
			if (!dst->name[i]) \
				return -ENOMEM; \
		} \
	}

#define JORM_CONTAINER_END \
	return 0; \
}

#define JORM_IGNORE(x)
