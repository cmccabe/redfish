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

#include "common//config/ostorc.h"

#define JORM_CUR_FILE "common/config/ostorc.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

#define DEFAULT_OSTOR_TIMEO 120

void harmonize_ostorc(struct ostorc *conf, char *err, size_t err_len)
{
	if (conf->ostor_max_open == JORM_INVAL_INT)
		conf->ostor_max_open = sysconf(_SC_OPEN_MAX);
	if (conf->ostor_timeo == JORM_INVAL_INT)
		conf->ostor_timeo = DEFAULT_OSTOR_TIMEO;
	if (conf->ostor_path == JORM_INVAL_STR) {
		snprintf(err, err_len, "you must give a path to the ostor");
		return;
	}
	if (conf->ostor_max_open < 0) {
		snprintf(err, err_len, "ostor->max_open cannot be less "
			"than 0");
		return;
	}
	if (conf->ostor_timeo < 0) {
		snprintf(err, err_len, "ostor->ostor_timeo cannot be less "
			"than 0");
		return;
	}
}
