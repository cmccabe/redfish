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
