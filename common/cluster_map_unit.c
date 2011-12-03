/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
"        \"base_dir\" : \"/home/cmccabe/oftmp/mds1\"",
"        } ],",
"\"osd\" : [ {",
"        \"host\" : \"127.0.0.1\",",
"        \"port\" : 9001,",
"        \"base_dir\" : \"/home/cmccabe/oftmp/osd1\",",
"        \"rack_id\" : 1",
"        } ]",
"}",
NULL
};

static int test_cmap_from_conf(const char *tdir)
{
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
	EXPECT_EQUAL(cmap->num_osd, 1);
	localhost = inet_addr("127.0.0.1");
	EXPECT_EQUAL(cmap->epoch, 1);
	EXPECT_EQUAL(cmap->oinfo[0].ip, localhost);
	EXPECT_EQUAL(cmap->oinfo[0].port, 9001);
	EXPECT_EQUAL(cmap->num_mds, 1);
	EXPECT_EQUAL(cmap->minfo[0].ip, localhost);
	EXPECT_EQUAL(cmap->minfo[0].port, 9000);
	cmap_free(cmap);
	return 0;
}

static int test_cmap_round_trip(struct cmap *cmap)
{
	int i;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct cmap *cmap2;
	size_t buf_len;
	char *buf;

	buf = cmap_to_buffer(cmap, &buf_len);
	EXPECT_NOT_EQUAL(buf, NULL);
	cmap2 = cmap_from_buffer(buf, buf_len, err, err_len);
	EXPECT_ZERO(err[0]);
	EXPECT_EQUAL(cmap->epoch, cmap2->epoch);
	EXPECT_EQUAL(cmap->num_osd, cmap2->num_osd);
	EXPECT_EQUAL(cmap->num_mds, cmap2->num_mds);
	for (i = 0; i < cmap->num_osd; ++i) {
		EXPECT_EQUAL(cmap->oinfo[i].ip, cmap2->oinfo[i].ip);
		EXPECT_EQUAL(cmap->oinfo[i].port, cmap2->oinfo[i].port);
		EXPECT_EQUAL(cmap->oinfo[i].port, cmap2->oinfo[i].port);
	}
	for (i = 0; i < cmap->num_mds; ++i) {
		EXPECT_EQUAL(cmap->minfo[i].ip, cmap2->minfo[i].ip);
		EXPECT_EQUAL(cmap->minfo[i].port, cmap2->minfo[i].port);
	}
	free(buf);
	cmap_free(cmap2);
	return 0;
}

static int test_cmap_round_trip_1(void)
{
	struct cmap *cmap;
	uint32_t localhost;

	cmap = calloc(1, sizeof(struct cmap));
	EXPECT_NOT_EQUAL(cmap, NULL);
	cmap->epoch = 123;
	cmap->num_osd = 2;
	cmap->num_mds = 2;
	cmap->oinfo = calloc(2, sizeof(struct daemon_info));
	EXPECT_NOT_EQUAL(cmap->oinfo, NULL);
	cmap->minfo = calloc(2, sizeof(struct daemon_info));
	EXPECT_NOT_EQUAL(cmap->minfo, NULL);
	localhost = inet_addr("127.0.0.1");
	cmap->oinfo[0].ip = localhost;
	cmap->oinfo[0].port = 8080;
	cmap->oinfo[1].ip = localhost;
	cmap->oinfo[1].port = 8081;
	cmap->minfo[0].ip = localhost;
	cmap->minfo[0].port = 9080;
	cmap->minfo[1].ip = localhost;
	cmap->minfo[1].port = 9081;
	
	EXPECT_ZERO(test_cmap_round_trip(cmap));
	cmap_free(cmap);

	return 0;
}

static int test_cmap_round_trip_2(void)
{
	struct cmap *cmap;
	uint32_t localhost;

	cmap = calloc(1, sizeof(struct cmap));
	EXPECT_NOT_EQUAL(cmap, NULL);
	cmap->epoch = 456;
	cmap->num_osd = 1;
	cmap->num_mds = 2;
	cmap->oinfo = calloc(1, sizeof(struct daemon_info));
	EXPECT_NOT_EQUAL(cmap->oinfo, NULL);
	cmap->minfo = calloc(2, sizeof(struct daemon_info));
	EXPECT_NOT_EQUAL(cmap->minfo, NULL);
	localhost = inet_addr("127.0.0.1");
	cmap->oinfo[0].ip = localhost;
	cmap->oinfo[0].port = 8080;
	cmap->minfo[0].ip = localhost;
	cmap->minfo[0].port = 9080;
	cmap->minfo[1].ip = localhost;
	cmap->minfo[1].port = 9081;
	
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
