/*
 * vim: ts=8:sw=8:tw=79:noet
 * 
 * Copyright 2012 the Redfish authors
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

#ifndef REDFISH_OSD_NET_DOT_H
#define REDFISH_OSD_NET_DOT_H

#include <stdint.h> /* for uint16_t */
#include <unistd.h> /* for size_t */

struct unitaryc;

/** Initialize osd networking stuff
 *
 * @param conf		The unitary Redfish configuration
 * @param oid		The osd ID
 * @param err		(out param) error message
 * @param err_len	length of error buffer
 */
extern void osd_net_init(struct unitaryc *conf, uint16_t oid,
		char *err, size_t err_len);

/** Runs the main object storage server loop
 *
 * @return	0 on successful exit; error code otherwise
 */
extern int osd_net_main_loop(void);

#endif
