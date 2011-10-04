/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_RSEM_RSEM_DOT_H
#define REDFISH_RSEM_RSEM_DOT_H

#define JORM_CUR_FILE "rsem/rsem.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "rsem/rsem.jorm"
#endif

/* Network messages we send and receive.
 * Some of them have corresponding JSON payloads. */
enum {
	RSEM_CLIENT_ACK,
	RSEM_CLIENT_REL_SEM,
	RSEM_CLIENT_REQ_SEM,
	RSEM_SERVER_ACK,
	RSEM_SERVER_NACK,
	RSEM_SERVER_DELAY_SEM,
	RSEM_SERVER_GIVE_SEM,
	RSEM_SERVER_INTERNAL_ERROR,
	RSEM_SERVER_NO_SUCH_SEM,
};

#endif
