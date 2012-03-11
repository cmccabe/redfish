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

#include "common/config/logc.h"
#include "common/config/mdsc.h"
#include "common/config/mstorc.h"

#define JORM_CUR_FILE "common/config/mdsc.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

#define MDSC_DEFAULT_MDS_PORT 7000
#define MDSC_DEFAULT_OSD_PORT 7001
#define MDSC_DEFAULT_CLI_PORT 7002

void harmonize_mdsc(struct mdsc *conf, char *err, size_t err_len)
{
	char path[PATH_MAX];

	harmonize_logc(conf->lc, err, err_len);
	if (err[0])
		return;
	/* If the user hasn't specified an mstor path, but base_dir is set,
	 * assume that the mstor is in there.  This is convenient for tests. */
	if ((conf->mc->mstor_path == JORM_INVAL_STR) &&
			(conf->lc->base_dir != JORM_INVAL_STR)) {
		if (!zsnprintf(path, sizeof(path), "%s/cur.mstor",
				conf->lc->base_dir)) {
			conf->mc->mstor_path = strdup(path);
		}
	}
	harmonize_mstorc(conf->mc, err, err_len);
	if (err[0])
		return;
	if (conf->cli_port == JORM_INVAL_INT)
		conf->cli_port = MDSC_DEFAULT_CLI_PORT;
	if (conf->mds_port == JORM_INVAL_INT)
		conf->mds_port = MDSC_DEFAULT_MDS_PORT;
	if (conf->osd_port == JORM_INVAL_INT)
		conf->mds_port = MDSC_DEFAULT_OSD_PORT;
	if (conf->host == JORM_INVAL_STR) {
		snprintf(err, err_len, "you must give a hostname");
		return;
	}
}
