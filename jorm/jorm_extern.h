/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

struct json_object;

#define JORM_CONTAINER_BEGIN(name) \
	struct name; \
	extern struct name *JORM_FROMJSON_##name(struct json_object *jo); \
	extern struct json_object *JORM_TOJSON_##name(struct name *me); \
	extern void JORM_FREE_##name(struct name *jorm);

#define JORM_INT(name)
#define JORM_DOUBLE(name)
#define JORM_STR(name)
#define JORM_NESTED(name, ty)
#define JORM_BOOL(name)
#define JORM_ARRAY(name, ty)
#define JORM_CONTAINER_END
#define JORM_IGNORE(x)

