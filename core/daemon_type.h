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

#ifndef REDFISH_CORE_DAEMON_TYPE_H
#define REDFISH_CORE_DAEMON_TYPE_H

enum fish_daemon_ty
{
	REDFISH_DAEMON_TYPE_OSD = 0,
	REDFISH_DAEMON_TYPE_MDS = 1,
	REDFISH_DAEMON_TYPE_MON = 2,
	REDFISH_DAEMON_TYPE_NUM = 3,
};

#endif
