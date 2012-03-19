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

#include "common/entity_type.h"

const char *fish_entity_ty_to_str(enum fish_entity_ty ty)
{
	switch (ty) {
	case RF_ENTITY_TY_MDS:
		return "RF_ENTITY_TY_MDS";
	case RF_ENTITY_TY_OSD:
		return "RF_ENTITY_TY_OSD";
	case RF_ENTITY_TY_CLI:
		return "RF_ENTITY_TY_CLI";
	default:
		return "(unknown)";
	}
}
