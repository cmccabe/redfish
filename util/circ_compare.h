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

#ifndef REDFISH_UTIL_CIRC_COMPARE_DOT_H
#define REDFISH_UTIL_CIRC_COMPARE_DOT_H

#include <stdint.h> /* for uint16_t, etc. */

/* Circular comparison functions
 *
 * When a counter can roll over, we may sometimes find that "incrementing" it
 * (in the CPU sense) leads to a number which is numerically smaller.  For
 * example, incrementing the 16-bit number 65535 gives us 0, which is certainly
 * less.
 *
 * So how can we prevent these shenanigans?  Well, we could use huge (64 bit?)
 * counters for everything, and hope for the best.  However, there is sometimes
 * a better way.  If we know that the difference between the two numbers being
 * compared is less than half of the total range of the counter, we can perform
 * a circular comparison.
 *
 * Some examples:
 *
 * =================================
 * A        B
 * Normal comparison result: A < B
 * Circular comparison result: A < B
 *
 * =================================
 * B        A
 * Normal comparison result: A > B
 * Circular comparison result: A > B
 *
 * =================================
 * B                             A
 * Normal comparison result: A > B
 * Circular comparison result: A < B
 */

/** Perform a circular comparison of two 16-bit numbers
 *
 * @param a		The first number
 * @param b		The second number
 *
 * @return		-1, 0, or 1 for a < b, a == b, or a > b
 */
extern int circ_compare16(uint16_t a, uint16_t b);

#endif
