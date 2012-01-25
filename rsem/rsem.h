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
