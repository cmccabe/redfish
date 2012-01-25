/**
 * Copyright 2011-2012 the RedFish authors
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
#include <string.h>

#include "client/fishc.h"
#include "hadoop/common.h"
#include "mds/limits.h"
#include "util/compiler.h"
#include "util/macro.h"

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

void Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishConnect(
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

jboolean Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishMkdirs(
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
		goto error;
	jgroup = (*jenv)->NewStringUTF(jenv, osa->group);
	if (!jgroup)
		goto error;
	jpath = (*jenv)->NewStringUTF(jenv, osa->path);
	if (!jpath)
		goto error;
	jpath_obj = (*jenv)->NewObject(jenv, g_cls_path,
			g_mid_path_ctor, jpath);
	if (!jpath_obj)
		goto error;
	jperm = (*jenv)->NewObject(jenv, g_cls_file_perm,
			g_mid_file_perm_ctor, (jshort)osa->mode);
	if (!jperm)
		goto error;
	length = osa->length;
	is_dir = osa->is_dir;
	repl = osa->repl;
	block_sz = osa->block_sz;
	mtime = osa->mtime;
	atime = osa->atime;
	jstat = (*jenv)->NewObject(jenv, g_cls_file_status,
		g_mid_file_status_ctor, length, is_dir, repl,
		block_sz, mtime, atime, jperm, jowner, jgroup, jpath_obj);
	if (!jstat)
		goto error;
	return jstat;

error:
	if (jstat)
		(*jenv)->DeleteLocalRef(jenv, jstat);
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
	return NULL;
}

jobject Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishGetPathStatus(
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

void Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishChmod(
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

void Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishChown(
		JNIEnv *jenv, jobject jobj, jstring jpath,
		jstring jowner, jstring jgroup)
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

void Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishUtimes(
		JNIEnv *jenv, jobject jobj, jstring jpath,
		jlong mtime, jlong atime)
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

jboolean Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishUnlink(
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

jboolean Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishUnlinkTree(
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

jboolean Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishRename(
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

void Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishDisconnect(
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

void Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishFree(
		JNIEnv *jenv, jobject jobj)
{
	struct redfish_client *cli;

	cli = redfish_get_m_cli(jenv, jobj);
	if (!cli)
		return;
	redfish_free_client(cli);
	redfish_set_m_cli(jenv, jobj, NULL);
}
