/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#define JORM_CONTAINER_BEGIN(name) struct name {
#define JORM_INT(name) int name;
#define JORM_DOUBLE(name) double name;
#define JORM_STR(name) char *name;
#define JORM_NESTED(name, ty) struct ty *name;
#define JORM_EMBEDDED(name, ty) struct ty *name;
#define JORM_BOOL(name) int name;
#define JORM_ARRAY(name, ty) struct ty **name;
#define JORM_CONTAINER_END };
#define JORM_IGNORE(x) x
