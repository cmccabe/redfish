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

#ifndef REDFISH_UTIL_CRAM_DOT_H
#define REDFISH_UTIL_CRAM_DOT_H

#include <stdint.h> /* for uint32_t */

/** Cram an integer into a u16.
 *
 * @param val		The integer
 *
 * @return		The uint16_t
 *			If the value is < 0, will return 0xffff. If the value is
 *			too big, will report 0xfffe.
 */
extern uint16_t cram_into_u16(int val);

/** Cram an integer into a u8.
 *
 * @param val		The integer
 *
 * @return		The uint8_t
 *			If the value is < 0, will return 0xff. If the value is
 *			too big, will report 0xfe.
 */
extern uint8_t cram_into_u8(int val);

#endif
