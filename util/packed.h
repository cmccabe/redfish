/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_PACKED_H
#define REDFISH_UTIL_PACKED_H

#include <stdint.h>

extern uint8_t pbe8_to_h(void *v);

extern void ph_to_be8(void *v, uint8_t u);

extern uint16_t pbe16_to_h(void *v);

extern void ph_to_be16(void *v, uint16_t u);

extern uint32_t pbe32_to_h(void *v);

extern void ph_to_be32(void *v, uint32_t u);

extern uint64_t pbe64_to_h(void *v);

extern void ph_to_be64(void *v, uint64_t u);


#endif
