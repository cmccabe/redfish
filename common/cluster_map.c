/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "common/cluster_map.h"
#include "common/config/unitaryc.h"
#include "util/compiler.h"
#include "util/net.h"
#include "util/packed.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Cluster map.
 *
 * See cluster_map.h for an overview of what this is used for.
 *
 */

/****************************** constants ********************************/

/********************************* types *****************************/
PACKED(struct packed_addr {
	uint32_t ip;
	uint16_t port;
	uint16_t pad;
});

PACKED(struct packed_cmap {
	uint64_t epoch;
	uint32_t num_osd;
	uint32_t num_mds;
	/* next: osd address array */
	/* next: mds address array */
});

/********************************* functions *****************************/
struct cmap *cmap_from_conf(const struct unitaryc *conf,
			char *err, size_t err_len)
{
	struct cmap *cmap = NULL;
	struct daemon_info *oinfo = NULL, *minfo = NULL;
	int i, num_osd = 0, num_mds = 0;
	struct osdc **osd;
	struct mdsc **mds;

	for (osd = conf->osd; *osd; ++osd) {
		num_osd++;
	}
	for (mds = conf->mds; *mds; ++mds) {
		num_mds++;
	}
	cmap = calloc(1, sizeof(struct cmap));
	if (!cmap)
		goto oom_error;
	oinfo = calloc(num_osd, sizeof(struct daemon_info));
	if (!oinfo)
		goto oom_error;
	for (i = 0; i < num_osd; ++i) {
		oinfo[i].ip = get_first_ipv4_addr(conf->osd[i]->host,
						err, err_len);
		if (err[0])
			goto error;
		oinfo[i].port = conf->osd[i]->port;
		oinfo[i].in = 1;
	}
	minfo = calloc(num_mds, sizeof(struct daemon_info));
	if (!minfo)
		goto oom_error;
	for (i = 0; i < num_mds; ++i) {
		minfo[i].ip = get_first_ipv4_addr(conf->mds[i]->host,
						err, err_len);
		if (err[0])
			goto error;
		minfo[i].port = conf->mds[i]->mds_port;
		minfo[i].in = 1;
	}
	cmap->epoch = 1;
	cmap->num_osd = num_osd;
	cmap->oinfo = oinfo;
	cmap->num_mds = num_mds;
	cmap->minfo = minfo;
	return cmap;

oom_error:
	snprintf(err, err_len, "cmap_from_conf: out of memory!");
error:
	free(oinfo);
	free(minfo);
	free(cmap);
	return NULL;
}

struct cmap *cmap_from_buffer(const char *buf, size_t buf_len,
				char *err, size_t err_len)
{
	struct cmap *cmap = NULL;
	struct daemon_info *oinfo = NULL, *minfo = NULL;
	int i, num_osd = 0, num_mds = 0;
	struct packed_cmap *hdr;
	struct packed_addr *pa;

	if (buf_len < sizeof(struct packed_cmap)) {
		snprintf(err, err_len, "Buffer was way too short to contain a "
			 "cluster map.  It only contained %Zd bytes!", buf_len);
		goto error;
	}
	cmap = calloc(1, sizeof(struct cmap));
	if (!cmap)
		goto oom_error;
	hdr = (struct packed_cmap*)buf;
	cmap->epoch = unpack_from_be64(&hdr->epoch);
	cmap->num_osd = unpack_from_be32(&hdr->num_osd);
	cmap->num_mds = unpack_from_be32(&hdr->num_mds);
	oinfo = calloc(num_osd, sizeof(struct daemon_info));
	if (!oinfo)
		goto oom_error;
	minfo = calloc(num_mds, sizeof(struct daemon_info));
	if (!minfo)
		goto oom_error;
	buf_len -= sizeof(struct packed_cmap);
	buf += sizeof(struct packed_cmap);
	for (i = 0; i < cmap->num_osd; ++i) {
		if (buf_len < sizeof(struct packed_addr)) {
			snprintf(err, err_len, "The buffer was too short "
				 "to contain all the OSD information.");
			goto error;
		}
		pa = (struct packed_addr*)buf;
		oinfo[i].ip = unpack_from_be32(&pa->ip);
		oinfo[i].port = unpack_from_be16(&pa->port);
		buf_len -= sizeof(struct packed_addr);
		buf += sizeof(struct packed_addr);
	}
	for (i = 0; i < cmap->num_mds; ++i) {
		if (buf_len < sizeof(struct packed_addr)) {
			snprintf(err, err_len, "The buffer was too short "
				 "to contain all the MDS information.");
			goto error;
		}
		pa = (struct packed_addr*)buf;
		minfo[i].ip = unpack_from_be32(&pa->ip);
		minfo[i].port = unpack_from_be16(&pa->port);
		buf_len -= sizeof(struct packed_addr);
		buf += sizeof(struct packed_addr);
	}
	cmap->oinfo = oinfo;
	cmap->minfo = minfo;
	return cmap;

oom_error:
	snprintf(err, err_len, "cmap_from_conf: out of memory!");
error:
	free(oinfo);
	free(minfo);
	free(cmap);
	return NULL;
}

char *cmap_to_buffer(const struct cmap *cmap, size_t *buf_len)
{
	char *buf, *b;
	int i;
	size_t len;
	struct packed_cmap *hdr;
	struct packed_addr *pa;

	len = sizeof(struct packed_cmap) +
		(cmap->num_osd * sizeof(struct packed_addr)) + 
		(cmap->num_mds * sizeof(struct packed_addr));
	buf = calloc(1, len);
	if (!buf)
		return NULL;
	*buf_len = len;
	b = buf;
	hdr = (struct packed_cmap*)buf;
	pack_to_be64(&hdr->epoch, cmap->epoch);
	pack_to_be32(&hdr->num_osd, cmap->num_osd);
	pack_to_be32(&hdr->num_mds, cmap->num_mds);
	b += sizeof(struct packed_cmap);
	for (i = 0; i < cmap->num_osd; ++i) {
		pa = (struct packed_addr*)b;
		pack_to_be32(&pa->ip, cmap->oinfo[i].ip);
		pack_to_be16(&pa->port, cmap->oinfo[i].port);
		b += sizeof(struct packed_addr);
	}
	for (i = 0; i < cmap->num_mds; ++i) {
		pa = (struct packed_addr*)b;
		pack_to_be32(&pa->ip, cmap->minfo[i].ip);
		pack_to_be16(&pa->port, cmap->minfo[i].port);
		b += sizeof(struct packed_addr);
	}
	return buf;
}

void cmap_free(struct cmap *cmap)
{
	if (!cmap)
		return;
	free(cmap->oinfo);
	free(cmap->minfo);
	free(cmap);
}
