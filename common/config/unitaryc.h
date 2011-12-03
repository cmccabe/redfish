/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_COMMON_CONFIG_UNITARYC_DOT_H
#define REDFISH_COMMON_CONFIG_UNITARYC_DOT_H

#include "common/config/mdsc.h"
#include "common/config/osdc.h"

#include <unistd.h> /* for size_t */

#define JORM_CUR_FILE "common/config/unitaryc.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "common/config/unitaryc.jorm"
#endif

/** Parse a unitary configuration file.
 *
 * @param fname		The file name to open
 * @param err		(out-param) the error message, on failure
 * @param err_len	length of the error buffer
 *
 * @return		the dynamically allocated unitary configuration data
 */
extern struct unitaryc *parse_unitary_conf_file(const char *fname,
					char *err, size_t err_len);

/** Free unitary configuration data.
 *
 * @param conf		The unitary configuration data
 */
extern void free_unitary_conf_file(struct unitaryc *conf);

#endif
