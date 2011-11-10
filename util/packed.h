/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_PACKED_H
#define REDFISH_UTIL_PACKED_H

#include <stdint.h> /* for uint32_t */

#include <unistd.h> /* for size_t */

extern uint8_t pbe8_to_h(void *v);

extern void ph_to_be8(void *v, uint8_t u);

extern uint16_t pbe16_to_h(void *v);

extern void ph_to_be16(void *v, uint16_t u);

extern uint32_t pbe32_to_h(void *v);

extern void ph_to_be32(void *v, uint32_t u);

extern uint64_t pbe64_to_h(void *v);

extern void ph_to_be64(void *v, uint64_t u);

/** Retrieve a packed NULL-terminated string
 *
 * @param v		Pointer to the data
 * @param off		(inout) offset to start at.  When the function finishes,
 *			will be the next offset to pull things from.
 * @param out		(out-param) where to put the string
 * @param out_len	length of the out buffer
 *
 * @return		0 on success.
 * 			-ENAMETOOLONG if the string we found was too long to fit
 * 			in the supplied buffer.
 * 			-EINVAL if we couldn't find a NULL before reaching the
 * 			end of the buffer.
 */
extern int pget_str(void *v, uint32_t *off, uint32_t len,
			char *out, size_t out_len);

/** Create a packed NULL-terminated string
 *
 * @param v		Pointer to the data
 * @param off		(inout) offset to start at.  When the function finishes,
 *			will be the next offset to push things to.
 * @param len		Total available length in the data buffer
 * @param str		The string to add
 *
 * @return		0 on success.
 * 			-ENAMETOOLONG if there wasn't enough memory to add
 * 			the string to the supplied buffer.
 */
extern int pput_str(void *v, uint32_t *off, uint32_t len, char *str);

#endif
