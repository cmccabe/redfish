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

struct json_object;

#include <unistd.h> /* for size_t */

#define JORM_CONTAINER_BEGIN(ty) \
	struct ty; \
	extern struct ty* JORM_INIT_##ty(void); \
	extern struct ty *JORM_FROMJSON_##ty(struct json_object *jo); \
	extern struct json_object *JORM_TOJSON_##ty(struct ty *me); \
	extern void JORM_FREE_##ty(struct ty *jorm); \
	extern int JORM_COPY_##ty(struct ty *src, struct ty *dst); \
	extern struct ty** JORM_ARRAY_COPY_##ty(struct ty **arr); \
	extern void JORM_TYCHECK_##ty(struct json_object *jo, char* acc, \
				size_t acc_len, char *err, size_t err_len); \
	extern struct ty* JORM_ARRAY_APPEND_##ty(struct ty ***arr); \
	extern void JORM_ARRAY_FREE_##ty(struct ty ***arr); \
	extern void JORM_ARRAY_REMOVE_##ty(struct ty ***arr, struct ty *elem);
#define JORM_INT(name)
#define JORM_DOUBLE(name)
#define JORM_STR(name)
#define JORM_NESTED(name, ty) \
	struct ty;
#define JORM_EMBEDDED(name, ty) \
	struct ty;
#define JORM_BOOL(name)
#define JORM_ARRAY(name, ty)
#define JORM_CONTAINER_END
#define JORM_IGNORE(x)

