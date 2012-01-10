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

#ifndef REDFISH_OSD_NET_DOT_H
#define REDFISH_OSD_NET_DOT_H

struct osdc;

/** Start the object storage daemon main loop
 *
 * @param conf		The daemon configuration
 *
 * @return		The return value of the program
 */
extern int osd_main_loop(struct osdc *conf);

#endif
