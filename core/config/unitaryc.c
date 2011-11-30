/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/config/logc.h"
#include "core/config/mdsc.h"
#include "core/config/osdc.h"
#include "core/config/unitaryc.h"

#define JORM_CUR_FILE "core/config/unitaryc.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

struct unitaryc* parse_unitary_conf_file(const char *fname, char *err, size_t err_len)
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

void free_unitary_conf_file(struct unitaryc *conf)
{
	JORM_FREE_unitaryc(conf);
}
