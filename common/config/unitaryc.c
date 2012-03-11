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
#include "common/config/osdc.h"
#include "common/config/unitaryc.h"

#define JORM_CUR_FILE "common/config/unitaryc.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

struct unitaryc* parse_unitary_conf_file(const char *fname,
		char *err, size_t err_len)
{
	char err2[512] = { 0 };
	size_t err2_len = sizeof(err2);
	struct unitaryc *conf;
	struct json_object* jo;

	jo = parse_json_file(fname, err2, err2_len);
	if (err2[0]) {
		snprintf(err, err_len, "error parsing json file %s: %s",
			fname, err2);
		return NULL;
	}
	conf = JORM_FROMJSON_unitaryc(jo);
	json_object_put(jo);
	if (!conf) {
		snprintf(err, err_len, "ran out of memory reading "
			"config file.\n");
		return NULL;
	}
	if (conf->osd == JORM_INVAL_ARRAY) {
		snprintf(err, err_len, "No OSDs found!");
		goto error;
	}
	if (conf->mds == JORM_INVAL_ARRAY) {
		snprintf(err, err_len, "No MDSes found!");
		goto error;
	}
	return conf;

error:
	JORM_FREE_unitaryc(conf);
	return NULL;
}

void harmonize_unitary_conf(struct unitaryc *conf, char *err, size_t err_len)
{
	int idx;
	struct mdsc **m;
	struct osdc **o;

	for (idx = 0, o = conf->osd; *o; ++o, ++idx) {
		harmonize_osdc(*o, err, err_len);
		if (err[0]) {
			snappend(err, err_len, "(OSD %d)", idx);
			return;
		}
	}
	for (idx = 0, m = conf->mds; *m; ++m, ++idx) {
		harmonize_mdsc(*m, err, err_len);
		if (err[0]) {
			snappend(err, err_len, "(MDS %d)", idx);
			return;
		}
	}
}

void free_unitary_conf_file(struct unitaryc *conf)
{
	JORM_FREE_unitaryc(conf);
}
