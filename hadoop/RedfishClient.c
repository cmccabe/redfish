/**
 * Copyright 2011-2012 the Redfish authors
 *
 * Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <errno.h>
#include <jni.h>
#include <limits.h>
#include <string.h>

#include "client/fishc.h"
#include "hadoop/common.h"
#include "mds/limits.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/macro.h"

/* Hadoop expects block names to be in the format <hostname>:<port-number>.
 * So the maximum length of this string is the host name length plus the maximum
 * length of a colon and a 16-bit number.
 */
#define RF_BLOCK_NAME_MAX (_POSIX_HOST_NAME_MAX + 7)

/* We must be able to fit a pointer into a Java 'long'.  Believe it or not, this
 * is the official way to store native pointers in Java structures.
 */
BUILD_BUG_ON(sizeof(jlong) < sizeof(void*));

static void redfish_set_m_cli(JNIEnv *jenv, jobject jobj, void *ptr)
{
	(*jenv)->SetLongField(jenv, jobj, g_fid_m_cli, (jlong)(uintptr_t)ptr);
}

static void* redfish_get_m_cli(JNIEnv *jenv, jobject jobj)
{
	return (void*)(uintptr_t)(*jenv)->GetLongField(jenv, jobj, g_fid_m_cli);
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishConnect(
	JNIEnv *jenv, jobject jobj, POSSIBLY_UNUSED(jstring jhost),
	POSSIBLY_UNUSED(jint jport), jstring juser)
{
	int ret = 0;
	char cuser[RF_USER_MAX];
	struct redfish_client *cli  = NULL;
	struct redfish_mds_locator **mlocs = NULL;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (cli != NULL) {
		/* already initialized */
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, juser, 0, sizeof(cuser), cuser);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	/** TODO: fix when we use conf files instead of host/port pairs */
	mlocs = redfish_mlocs_from_str("", err, err_len);
	if (err[0])
		goto done;
	ret = redfish_connect(mlocs, cuser, &cli);
	if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
	redfish_set_m_cli(jenv, jobj, cli);
done:
	if (mlocs)
		redfish_mlocs_free(mlocs);
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
}

JNIEXPORT jobject JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishOpen(
	JNIEnv *jenv, jobject jobj, jstring jpath)
{
	jobject jstream = NULL;
	int ret = 0;
	char cpath[RF_PATH_MAX];
	struct redfish_client *cli;
	struct redfish_file *ofe = NULL;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (cli != NULL) {
		/* already initialized */
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	ret = redfish_open(cli, cpath, &ofe);
	if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
	jstream = (*jenv)->NewObject(jenv, g_cls_rf_in_stream,
			g_mid_rf_in_stream_ctor, (jlong)(uintptr_t)ofe);
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return jstream;
}

JNIEXPORT jobject JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishCreate(
	JNIEnv *jenv, jobject jobj, jstring jpath, jshort mode,
	jint bufsz, jshort repl, jint blocksz)
{
	jobject jstream = NULL;
	int ret = 0;
	char cpath[RF_PATH_MAX];
	struct redfish_client *cli;
	struct redfish_file *ofe = NULL;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (cli != NULL) {
		/* already initialized */
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	ret = redfish_create(cli, cpath, mode, bufsz, repl, blocksz, &ofe);
	if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
	jstream = (*jenv)->NewObject(jenv, g_cls_rf_out_stream,
			g_mid_rf_out_stream_ctor, (jlong)(uintptr_t)ofe);
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return jstream;
}

JNIEXPORT jboolean JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishMkdirs(
	JNIEnv *jenv, jobject jobj, jstring jpath, jshort jmode)
{
	int ret = 0;
	char cpath[RF_PATH_MAX];
	struct redfish_client *cli;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	ret = redfish_mkdirs(cli, jmode, cpath);
	if (ret)
		goto done;
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return (ret == 0);
}

static jobject redfish_block_loc_to_java(JNIEnv *jenv,
		const struct redfish_block_loc *blc)
{
	int i;
	char c_name[RF_BLOCK_NAME_MAX];
	jobjectArray host_arr = NULL, name_arr = NULL;
	jstring host = NULL, name = NULL;
	jobject jblc = NULL;

	host_arr = (*jenv)->NewObjectArray(jenv, blc->nhosts, g_cls_string, NULL);
	if (!host_arr)
		goto done;
	name_arr = (*jenv)->NewObjectArray(jenv, blc->nhosts, g_cls_string, NULL);
	if (!name_arr)
		goto done;

	for (i = 0; i < blc->nhosts; ++i) {
		host = (*jenv)->NewStringUTF(jenv, blc->hosts[i].hostname);
		if (!host)
			goto done;
		(*jenv)->SetObjectArrayElement(jenv, host_arr, i, host);
		(*jenv)->DeleteLocalRef(jenv, host);
		host = NULL;

		snprintf(c_name, RF_BLOCK_NAME_MAX, "%s:%d",
			blc->hosts[i].hostname, blc->hosts[i].port);
		name = (*jenv)->NewStringUTF(jenv, c_name);
		if (!name)
			goto done;
		(*jenv)->SetObjectArrayElement(jenv, name_arr, i, name);
		(*jenv)->DeleteLocalRef(jenv, name);
		name = NULL;
	}
	jblc = (*jenv)->NewObject(jenv, g_cls_block_loc,
			g_mid_block_loc_ctor, name_arr, host_arr,
			blc->start, blc->len);
done:
	if (host_arr)
		(*jenv)->DeleteLocalRef(jenv, host_arr);
	if (name_arr)
		(*jenv)->DeleteLocalRef(jenv, name_arr);
	if (host)
		(*jenv)->DeleteLocalRef(jenv, host);
	if (name)
		(*jenv)->DeleteLocalRef(jenv, name);
	return jblc;
}

JNIEXPORT jobjectArray JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishGetBlockLocations(
	JNIEnv *jenv, jobject jobj, jstring jpath, jlong start, jlong len)
{
	int i;
	jobject jitem;
	jobjectArray jarr = NULL;
	int nblc = 0;
	char cpath[RF_PATH_MAX];
	struct redfish_client *cli;
	struct redfish_block_loc **blcs = NULL;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		redfish_throw(jenv, "java/io/IOException", err);
		return NULL;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		return NULL;
	nblc = redfish_locate(cli, cpath, start, len, &blcs);
	if (nblc < 0) {
		strerror_r(FORCE_POSITIVE(nblc), err, err_len);
		redfish_throw(jenv, "java/io/IOException", err);
		return NULL;
	}
	jarr = (*jenv)->NewObjectArray(jenv, nblc, g_cls_block_loc, NULL);
	if (!jarr) {
		/* OOM exception raised by JVM */
		redfish_free_block_locs(blcs, nblc);
		return NULL;
	}
	for (i = 0; i < nblc; ++i) {
		jitem = redfish_block_loc_to_java(jenv, blcs[i]);
		if (!jitem) {
			/* exception raised by JVM */
			redfish_free_block_locs(blcs, nblc);
			(*jenv)->DeleteLocalRef(jenv, jarr);
			return NULL;
		}
		(*jenv)->SetObjectArrayElement(jenv, jitem, i, jarr);
		(*jenv)->DeleteLocalRef(jenv, jitem);
	}
	redfish_free_block_locs(blcs, nblc);
	return jarr;
}

static jobject redfish_stat_to_file_info(JNIEnv *jenv,
		const struct redfish_stat *osa)
{
	jlong length;
	jboolean is_dir;
	jint repl;
	jlong block_sz, mtime, atime;
	jstring jowner = NULL, jgroup = NULL, jpath = NULL;
	jobject jpath_obj = NULL, jperm = NULL, jstat = NULL;

	jowner = (*jenv)->NewStringUTF(jenv, osa->owner);
	if (!jowner)
		goto done;
	jgroup = (*jenv)->NewStringUTF(jenv, osa->group);
	if (!jgroup)
		goto done;
	jpath = (*jenv)->NewStringUTF(jenv, osa->path);
	if (!jpath)
		goto done;
	jpath_obj = (*jenv)->NewObject(jenv, g_cls_path,
			g_mid_path_ctor, jpath);
	if (!jpath_obj)
		goto done;
	jperm = (*jenv)->NewObject(jenv, g_cls_file_perm,
			g_mid_file_perm_ctor, (jshort)osa->mode);
	if (!jperm)
		goto done;
	length = osa->length;
	is_dir = osa->is_dir;
	repl = osa->repl;
	block_sz = osa->block_sz;
	mtime = osa->mtime;
	atime = osa->atime;
	jstat = (*jenv)->NewObject(jenv, g_cls_file_status,
		g_mid_file_status_ctor, length, is_dir, repl,
		block_sz, mtime, atime, jperm, jowner, jgroup, jpath_obj);

done:
	if (jperm)
		(*jenv)->DeleteLocalRef(jenv, jperm);
	if (jpath_obj)
		(*jenv)->DeleteLocalRef(jenv, jpath_obj);
	if (jpath)
		(*jenv)->DeleteLocalRef(jenv, jpath);
	if (jgroup)
		(*jenv)->DeleteLocalRef(jenv, jgroup);
	if (jowner)
		(*jenv)->DeleteLocalRef(jenv, jowner);
	return jstat;
}

JNIEXPORT jobject JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishGetPathStatus(
	JNIEnv *jenv, jobject jobj, jstring jpath)
{
	int ret = 0;
	jobject res = NULL;
	struct redfish_client *cli;
	struct redfish_stat osa;
	char cpath[RF_PATH_MAX], err[512] = { 0 };
	size_t err_len = sizeof(err);

	memset(&osa, 0, sizeof(osa));
	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	ret = redfish_get_path_status(cli, cpath, &osa);
	if (ret == -ENOENT) {
		snprintf(err, err_len, "No such file as '%s'", cpath);
		redfish_throw(jenv, "java/io/FileNotFound", err);
		err[0] = '\0';
		goto done;
	}
	else if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
	res = redfish_stat_to_file_info(jenv, &osa);
	if (!res) {
		strerror_r(ENOMEM, err, err_len);
		goto done;
	}
done:
	redfish_free_path_status(&osa);
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return res;
}

JNIEXPORT jobjectArray JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishListDirectory(
	JNIEnv *jenv, jobject jobj, jstring jpath)
{
	jobjectArray jarr = NULL;
	int i, nosa;
	struct redfish_client *cli;
	struct redfish_stat *osa = NULL;
	char cpath[RF_PATH_MAX], err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	nosa = redfish_list_directory(cli, cpath, &osa);
	if (nosa < 0) {
		strerror_r(nosa, err, err_len);
		goto done;
	}
	jarr = (*jenv)->NewObjectArray(jenv, nosa, g_cls_file_status, NULL);
	if (!jarr) {
		/* out of memory exception thrown */
		goto done;
	}
	for (i = 0; i < nosa; ++i) {
		jobject res;

		res = redfish_stat_to_file_info(jenv, osa + i);
		if (!res) {
			strerror_r(ENOMEM, err, err_len);
			goto done;
		}
		(*jenv)->SetObjectArrayElement(jenv, jarr, i, res);
		(*jenv)->DeleteLocalRef(jenv, res);
	}

done:
	if (osa)
		redfish_free_path_statuses(osa, nosa);
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return jarr;
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishChmod(
	JNIEnv *jenv, jobject jobj, jstring jpath, jshort jmode)
{
	int ret;
	char cpath[RF_PATH_MAX];
	struct redfish_client *cli;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	ret = redfish_chmod(cli, cpath, jmode);
	if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishChown(
	JNIEnv *jenv, jobject jobj, jstring jpath, jstring jowner,
	jstring jgroup)
{
	int ret;
	char cpath[RF_PATH_MAX], err[512] = { 0 };
	struct redfish_client *cli;
	char cowner[RF_USER_MAX] = { 0 }, cgroup[RF_GROUP_MAX] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	if (jowner) {
		(*jenv)->GetStringUTFRegion(jenv, jowner, 0,
				sizeof(cowner), cowner);
		if ((*jenv)->ExceptionCheck(jenv))
			goto done;
	}
	if (jgroup) {
		(*jenv)->GetStringUTFRegion(jenv, jgroup, 0,
				sizeof(cgroup), cgroup);
		if ((*jenv)->ExceptionCheck(jenv))
			goto done;
	}
	ret = redfish_chown(cli, cpath,
		(cowner[0] ? cowner : NULL), (cgroup[0] ? cgroup : NULL));
	if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishUtimes(
	JNIEnv *jenv, jobject jobj, jstring jpath, jlong mtime, jlong atime)
{
	int ret;
	struct redfish_client *cli;
	char cpath[RF_PATH_MAX], err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	ret = redfish_utimes(cli, cpath, mtime, atime);
	if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
}

JNIEXPORT jboolean JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishUnlink(
		JNIEnv *jenv, jobject jobj, jstring jpath)
{
	int ret;
	struct redfish_client *cli;
	char cpath[RF_PATH_MAX], err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	ret = redfish_unlink(cli, cpath);
	if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return (ret == 0);
}

JNIEXPORT jboolean JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishUnlinkTree(
	JNIEnv *jenv, jobject jobj, jstring jpath)
{
	int ret;
	struct redfish_client *cli;
	char cpath[RF_PATH_MAX], err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jpath, 0, sizeof(cpath), cpath);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	ret = redfish_unlink_tree(cli, cpath);
	if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return (ret == 0);
}

JNIEXPORT jboolean JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishRename(
	JNIEnv *jenv, jobject jobj, jstring jsrc, jstring jdst)
{
	int ret;
	struct redfish_client *cli;
	char csrc[RF_PATH_MAX], cdst[RF_PATH_MAX], err[512] = { 0 };
	size_t err_len = sizeof(err);

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	(*jenv)->GetStringUTFRegion(jenv, jsrc, 0, sizeof(csrc), csrc);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	(*jenv)->GetStringUTFRegion(jenv, jdst, 0, sizeof(cdst), cdst);
	if ((*jenv)->ExceptionCheck(jenv))
		goto done;
	ret = redfish_rename(cli, csrc, cdst);
	if (ret) {
		strerror_r(ret, err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return (ret == 0);
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishDisconnect(
	JNIEnv *jenv, jobject jobj)
{
	struct redfish_client *cli;

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli) {
		/* client was already freed */
		return;
	}
	redfish_disconnect(cli);
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishFree(
	JNIEnv *jenv, jobject jobj)
{
	struct redfish_client *cli;

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli)
		return;
	redfish_release_client(cli);
	redfish_set_m_cli(jenv, jobj, NULL);
}
