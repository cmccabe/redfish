/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
				size_t acc_len, char *err, size_t err_len);
#define JORM_INT(name)
#define JORM_DOUBLE(name)
#define JORM_STR(name)
#define JORM_NESTED(name, ty) \
	struct ty;
#define JORM_EMBEDDED(name, ty) \
	struct ty;
#define JORM_BOOL(name)
#define JORM_ARRAY(name, ty) \
	extern struct ty* JORM_ARRAY_APPEND_##ty(struct ty ***arr); \
	extern void JORM_ARRAY_FREE_##ty(struct ty ***arr);
#define JORM_CONTAINER_END
#define JORM_IGNORE(x)

