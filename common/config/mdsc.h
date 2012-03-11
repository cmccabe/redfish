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

#ifndef REDFISH_COMMON_CONFIG_MDSC_DOT_H
#define REDFISH_COMMON_CONFIG_MDSC_DOT_H

#define JORM_CUR_FILE "common/config/mdsc.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "common/config/mdsc.jorm"
#endif

/** Harmonize the MDS configuration
 *
 * @param conf		The MDS configuration
 * @param err		(out param) The error buffer
 * @param err_len	Length of the error buffer
 */
extern void harmonize_mdsc(struct mdsc *conf, char *err, size_t err_len);

#endif
