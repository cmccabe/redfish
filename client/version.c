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

#include "client/fishc.h"
#include "util/macro.h"

#include <stdint.h>

#define RF_VERSION_MAJOR 1
#define RF_VERSION_MINOR 0
#define RF_VERSION_PATCHLEVEL 0

#define RF_VERSION_STR \
	TO_STR2(RF_VERSION_MAJOR) "." \
	TO_STR2(RF_VERSION_MINOR) "." \
	TO_STR2(RF_VERSION_PATCHLEVEL)

struct redfish_version redfish_get_version(void)
{
	struct redfish_version version;

	version.major = RF_VERSION_MAJOR;
	version.minor = RF_VERSION_MINOR;
	version.patchlevel = RF_VERSION_PATCHLEVEL;
	return version;
}

const char* redfish_get_version_str(void)
{
	return RF_VERSION_STR;
}
