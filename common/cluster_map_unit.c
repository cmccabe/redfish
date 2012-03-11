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

#include "common/cluster_map.h"
#include "common/config/unitaryc.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

static const char *CONF1[] = {
"{",
"\"mds\" : [ {",
"        \"host\" : \"127.0.0.1\",",
"        \"mds_port\" : 9000,",
"        \"osd_port\" : 9001,",
"        \"cli_port\" : 9002,",
"        \"base_dir\" : \"/home/cmccabe/oftmp/mds1\"",
"        } ],",
"\"osd\" : [ {",
"        \"host\" : \"127.0.0.1\",",
"        \"mds_port\" : 9100,",
"        \"osd_port\" : 9101,",
"        \"cli_port\" : 9102,",
"        \"base_dir\" : \"/home/cmccabe/oftmp/osd1\",",
"        \"rack_id\" : 1",
"        } ]",
"}",
NULL
};

static int test_cmap_from_conf(const char *tdir)
{
	int i;
	struct cmap *cmap;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	char fname[PATH_MAX];
	struct unitaryc* conf;
	uint32_t localhost;

	EXPECT_ZERO(zsnprintf(fname, sizeof(fname), "%s/conf1", tdir));
	write_linearray_to_file(fname, CONF1, err, err_len);
	EXPECT_ZERO(err[0]);
	conf = parse_unitary_conf_file(fname, err, err_len);
	EXPECT_ZERO(err[0]);
	cmap  = cmap_from_conf(conf, err, err_len);
	EXPECT_ZERO(err[0]);
	EXPECT_EQ(cmap->num_osd, 1);
	localhost = ntohl(inet_addr("127.0.0.1"));
	EXPECT_EQ(cmap->epoch, 1);
	EXPECT_EQ(cmap->oinfo[0].ip, localhost);
	for (i = 0; i < RF_ENTITY_TY_NUM; ++i) {
		EXPECT_EQ(cmap->oinfo[0].port[i], 9100 + i);
	}
	EXPECT_EQ(cmap->oinfo[0].in, 1);
	EXPECT_EQ(cmap->num_mds, 1);
	EXPECT_EQ(cmap->minfo[0].ip, localhost);
	for (i = 0; i < RF_ENTITY_TY_NUM; ++i) {
		EXPECT_EQ(cmap->minfo[0].port[i], 9000 + i);
	}
	EXPECT_EQ(cmap->minfo[0].in, 1);
	cmap_free(cmap);
	free_unitary_conf_file(conf);
	return 0;
}

static int test_cmap_round_trip(struct cmap *cmap)
{
	int i, j;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct cmap *cmap2;
	size_t buf_len;
	char *buf;

	buf = cmap_to_buffer(cmap, &buf_len);
	EXPECT_NOT_EQ(buf, NULL);
	cmap2 = cmap_from_buffer(buf, buf_len, err, err_len);
	EXPECT_ZERO(err[0]);
	EXPECT_EQ(cmap->epoch, cmap2->epoch);
	EXPECT_EQ(cmap->num_osd, cmap2->num_osd);
	EXPECT_EQ(cmap->num_mds, cmap2->num_mds);
	for (i = 0; i < cmap->num_osd; ++i) {
		EXPECT_EQ(cmap->oinfo[i].ip, cmap2->oinfo[i].ip);
		for (j = 0; j < RF_ENTITY_TY_NUM; ++j) {
			EXPECT_EQ(cmap->oinfo[i].port[j],
				cmap2->oinfo[i].port[j]);
		}
	}
	for (i = 0; i < cmap->num_mds; ++i) {
		EXPECT_EQ(cmap->minfo[i].ip, cmap2->minfo[i].ip);
		for (j = 0; j < RF_ENTITY_TY_NUM; ++j) {
			EXPECT_EQ(cmap->minfo[i].port[j],
				cmap2->minfo[i].port[j]);
		}
	}
	free(buf);
	cmap_free(cmap2);
	return 0;
}

static int test_cmap_round_trip_1(void)
{
	int j;
	struct cmap *cmap;
	uint32_t localhost;

	cmap = calloc(1, sizeof(struct cmap));
	EXPECT_NOT_EQ(cmap, NULL);
	cmap->epoch = 123;
	cmap->num_mds = 2;
	cmap->num_osd = 2;
	cmap->minfo = calloc(2, sizeof(struct daemon_info));
	EXPECT_NOT_EQ(cmap->minfo, NULL);
	cmap->oinfo = calloc(2, sizeof(struct daemon_info));
	EXPECT_NOT_EQ(cmap->oinfo, NULL);
	localhost = ntohl(inet_addr("127.0.0.1"));
	cmap->minfo[0].ip = localhost;
	for (j = 0; j < RF_ENTITY_TY_NUM; ++j) {
		cmap->minfo[1].port[j] = 8080 + j;
	}
	cmap->minfo[0].in = 0;
	cmap->minfo[1].ip = localhost;
	for (j = 0; j < RF_ENTITY_TY_NUM; ++j) {
		cmap->minfo[1].port[j] = 8090 + j;
	}
	cmap->minfo[1].in = 1;
	cmap->oinfo[0].ip = localhost;
	for (j = 0; j < RF_ENTITY_TY_NUM; ++j) {
		cmap->oinfo[0].port[j] = 8180 + j;
	}
	cmap->oinfo[0].in = 1;
	cmap->oinfo[1].ip = localhost;
	for (j = 0; j < RF_ENTITY_TY_NUM; ++j) {
		cmap->oinfo[1].port[j] = 8190 + j;
	}
	cmap->oinfo[1].in = 1;
	EXPECT_EQ(cmap_get_leader_mid(cmap), 1);
	EXPECT_ZERO(test_cmap_round_trip(cmap));
	cmap_free(cmap);

	return 0;
}

static int test_cmap_round_trip_2(void)
{
	int j;
	struct cmap *cmap;
	uint32_t localhost;

	cmap = calloc(1, sizeof(struct cmap));
	EXPECT_NOT_EQ(cmap, NULL);
	cmap->epoch = 456;
	cmap->num_osd = 1;
	cmap->num_mds = 2;
	cmap->minfo = calloc(2, sizeof(struct daemon_info));
	EXPECT_NOT_EQ(cmap->minfo, NULL);
	cmap->oinfo = calloc(1, sizeof(struct daemon_info));
	EXPECT_NOT_EQ(cmap->oinfo, NULL);
	localhost = ntohl(inet_addr("127.0.0.1"));
	cmap->minfo[0].ip = localhost;
	for (j = 0; j < RF_ENTITY_TY_NUM; ++j) {
		cmap->minfo[0].port[j] = 8080 + j;
	}
	cmap->minfo[0].in = 1;
	cmap->minfo[1].ip = localhost;
	for (j = 0; j < RF_ENTITY_TY_NUM; ++j) {
		cmap->minfo[1].port[j] = 8090 + j;
	}
	cmap->minfo[1].in = 1;
	cmap->oinfo[0].ip = localhost;
	for (j = 0; j < RF_ENTITY_TY_NUM; ++j) {
		cmap->oinfo[0].port[j] = 8180 + j;
	}
	cmap->oinfo[0].in = 1;
	EXPECT_EQ(cmap_get_leader_mid(cmap), 0);
	EXPECT_ZERO(test_cmap_round_trip(cmap));
	cmap_free(cmap);

	return 0;
}

int main(void)
{
	char tdir[PATH_MAX];

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(test_cmap_from_conf(tdir));
	EXPECT_ZERO(test_cmap_round_trip_1());
	EXPECT_ZERO(test_cmap_round_trip_2());

	return EXIT_SUCCESS;
}
