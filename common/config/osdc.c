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

#include "common//config/logc.h"
#include "common//config/osdc.h"

#define JORM_CUR_FILE "common/config/osdc.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

#define OSDC_DEFAULT_MDS_PORT 7100
#define OSDC_DEFAULT_OSD_PORT 7101
#define OSDC_DEFAULT_CLI_PORT 7102
#define DEFAULT_OSD_RACK 0

void harmonize_osdc(struct osdc *conf, char *err, size_t err_len)
{
	if (conf->mds_port == JORM_INVAL_INT)
		conf->mds_port = OSDC_DEFAULT_MDS_PORT;
	if (conf->osd_port == JORM_INVAL_INT)
		conf->osd_port = OSDC_DEFAULT_OSD_PORT;
	if (conf->cli_port == JORM_INVAL_INT)
		conf->cli_port = OSDC_DEFAULT_CLI_PORT;
	if (conf->rack == JORM_INVAL_INT)
		conf->rack = DEFAULT_OSD_RACK;
	if (conf->host == JORM_INVAL_STR) {
		snprintf(err, err_len, "You must give a hostname");
		return;
	}
}
