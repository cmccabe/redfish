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

#ifndef REDFISH_COMMON_ENTITY_TYPE_DOT_H
#define REDFISH_COMMON_ENTITY_TYPE_DOT_H

/** Represents a Redfish entity */
enum fish_entity_ty {
	/** MDS type */
	RF_ENTITY_TY_MDS = 0,
	/** OSD type */
	RF_ENTITY_TY_OSD = 1,
	/** Number of Redfish daemon types */
	RF_ENTITY_NUM_DAEMON_TY = 2,
	/** client type */
	RF_ENTITY_TY_CLI = 2,
	/** Number of entity types */
	RF_ENTITY_TY_NUM = 3,
};

#endif
