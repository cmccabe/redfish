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
#include "util/compiler.h"
#include "util/net.h"
#include "util/packed.h"
#include "util/string.h"

#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
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
	uint16_t in;
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

static void daemon_info_to_str(const struct daemon_info *info,
		char *buf, size_t *off, size_t buf_len)
{
	char ip_str[INET_ADDRSTRLEN];

	ipv4_to_str(info->ip, ip_str, sizeof(ip_str));
	fwdprintf(buf, off, buf_len, "\"%s:%d:%s\"", ip_str,
		info->port, (info->in ? "IN" : "OUT"));
}

int cmap_to_str(const struct cmap *cmap, char *buf, size_t buf_len)
{
	int i;
	size_t off;
	const char *prefix;

	off = 0;
	fwdprintf(buf, &off, buf_len, "{ \"epoch\": %"PRId64", ", cmap->epoch);
	fwdprintf(buf, &off, buf_len, "\"osd\": [");
	prefix = "";
	for (i = 0; i < cmap->num_osd; ++i) {
		fwdprintf(buf, &off, buf_len, "%s", prefix);
		daemon_info_to_str(&cmap->oinfo[i], buf, &off, buf_len);
		prefix = ", ";
	}
	fwdprintf(buf, &off, buf_len, "], \"mds\": [");
	prefix = "";
	for (i = 0; i < cmap->num_osd; ++i) {
		fwdprintf(buf, &off, buf_len, "%s", prefix);
		daemon_info_to_str(&cmap->minfo[i], buf, &off, buf_len);
		prefix = ", ";
	}
	fwdprintf(buf, &off, buf_len, "] }");
	return (off == buf_len) ?  -ENAMETOOLONG : 0;
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
	num_osd = unpack_from_be32(&hdr->num_osd);
	num_mds = unpack_from_be32(&hdr->num_mds);
	cmap->num_osd = num_osd;
	cmap->num_mds = num_mds;
	oinfo = calloc(num_osd, sizeof(struct daemon_info));
	if (!oinfo)
		goto oom_error;
	minfo = calloc(num_mds, sizeof(struct daemon_info));
	if (!minfo)
		goto oom_error;
	buf_len -= sizeof(struct packed_cmap);
	buf += sizeof(struct packed_cmap);
	for (i = 0; i < num_osd; ++i) {
		if (buf_len < sizeof(struct packed_addr)) {
			snprintf(err, err_len, "The buffer was too short "
				 "to contain all the OSD information.");
			goto error;
		}
		pa = (struct packed_addr*)buf;
		oinfo[i].ip = unpack_from_be32(&pa->ip);
		oinfo[i].port = unpack_from_be16(&pa->port);
		oinfo[i].in = unpack_from_be16(&pa->in);
		oinfo[i].send_time = 0;
		oinfo[i].recv_time = 0;
		buf_len -= sizeof(struct packed_addr);
		buf += sizeof(struct packed_addr);
	}
	for (i = 0; i < num_mds; ++i) {
		if (buf_len < sizeof(struct packed_addr)) {
			snprintf(err, err_len, "The buffer was too short "
				 "to contain all the MDS information.");
			goto error;
		}
		pa = (struct packed_addr*)buf;
		minfo[i].ip = unpack_from_be32(&pa->ip);
		minfo[i].port = unpack_from_be16(&pa->port);
		minfo[i].in = unpack_from_be16(&pa->in);
		minfo[i].send_time = 0;
		minfo[i].recv_time = 0;
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
		pack_to_be16(&pa->in, cmap->oinfo[i].in);
		b += sizeof(struct packed_addr);
	}
	for (i = 0; i < cmap->num_mds; ++i) {
		pa = (struct packed_addr*)b;
		pack_to_be32(&pa->ip, cmap->minfo[i].ip);
		pack_to_be16(&pa->port, cmap->minfo[i].port);
		pack_to_be16(&pa->in, cmap->minfo[i].in);
		b += sizeof(struct packed_addr);
	}
	return buf;
}

int cmap_get_leader_mid(const struct cmap *cmap)
{
	int i, num_mds;

	num_mds = cmap->num_mds;
	for (i = 0; i < num_mds; ++i) {
		if (cmap->minfo[i].in)
			return i;
	}
	return -1;
}

void cmap_free(struct cmap *cmap)
{
	if (!cmap)
		return;
	free(cmap->oinfo);
	free(cmap->minfo);
	free(cmap);
}
