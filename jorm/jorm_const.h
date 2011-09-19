/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef JORM_CONST_DOT_H
#define JORM_CONST_DOT_H

#include <limits.h> /* for MIN_INT */
#include <math.h> /* for INFINITY */

/* define JORM constants */
#define JORM_INVAL_BOOL			INT_MIN
#define JORM_INVAL_DOUBLE		INFINITY
#define JORM_INVAL_INT			INT_MIN
#define JORM_INVAL_NESTED		((void*)NULL)
#define JORM_INVAL_EMBEDDED		((void*)NULL)
#define JORM_INVAL_ARRAY		((void*)NULL)
#define JORM_INVAL_STR			((char*)NULL)

#endif
