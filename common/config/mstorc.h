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

#ifndef REDFISH_COMMON_CONFIG_MSTORC_DOT_H
#define REDFISH_COMMON_CONFIG_MSTORC_DOT_H

#define JORM_CUR_FILE "common/config/mstorc.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "common/config/mstorc.jorm"
#endif

/** Harmonize the mstor configuration
 *
 * @param conf		The mstor configuration
 * @param err		(out param) The error buffer
 * @param err_len	Length of the error buffer
 */
extern void harmonize_mstorc(struct mstorc *conf, char *err, size_t err_len);

#endif
