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
#include <stdlib.h>
#include <string.h>

#include "client/fishc.h"
#include "hadoop/common.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/macro.h"

#define RF_DOUTSTREAM_MALLOC_THRESH 8192

BUILD_BUG_ON(sizeof(jlong) < sizeof(void*));
BUILD_BUG_ON(sizeof(jlong) != sizeof(int64_t));
BUILD_BUG_ON(sizeof(jint) != sizeof(int32_t));

static void redfish_set_m_ofe(JNIEnv *jenv, jobject jobj, void *ptr)
{
	(*jenv)->SetLongField(jenv, jobj, g_fid_rf_out_stream_m_ofe,
			(jlong)(uintptr_t)ptr);
}

static void* redfish_get_m_ofe(JNIEnv *jenv, jobject jobj)
{
	return (void*)(uintptr_t)(*jenv)->GetLongField(jenv, jobj,
			g_fid_rf_out_stream_m_ofe);
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishDataOutputStream_close(
	JNIEnv *jenv, jobject jobj)
{
	char err[512];
	size_t err_len = sizeof(err);
	int ret;
	struct redfish_file *ofe;

	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe) {
		redfish_throw(jenv, "java/io/IOException", "m_ofe == NULL");
		return;
	}
	ret = redfish_close(ofe);
	if (ret) {
		strerror_r(FORCE_POSITIVE(ret), err, err_len);
		redfish_throw(jenv, "java/io/IOException", err);
		return;
	}
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishDataOutputStream_redfishFree(
	JNIEnv *jenv, jobject jobj)
{
	struct redfish_file *ofe;

	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe)
		return;
	redfish_free_file(ofe);
	redfish_set_m_ofe(jenv, jobj, NULL);
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishDataOutputStream_write(
	JNIEnv *jenv, jobject jobj, jbyteArray jarr, jint boff, jint blen)
{
	jint ret = -1;
	int8_t *cbuf = NULL;
	int8_t stack_buf[(blen <= RF_DOUTSTREAM_MALLOC_THRESH) ? blen : 1];
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct redfish_file *ofe;

	ret = validate_rw_params(jenv, jarr, boff, blen);
	if (ret)
		return;
	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe) {
		redfish_throw(jenv, "java/io/IOException", "m_ofe == NULL");
		return;
	}
	if (blen > RF_DOUTSTREAM_MALLOC_THRESH) {
		cbuf = malloc(blen);
		if (!cbuf) {
			strerror_r(ENOMEM, err, err_len);
			redfish_throw(jenv, "java/io/IOException", err);
			return;
		}
	}
	else {
		cbuf = stack_buf;
	}
	(*jenv)->GetByteArrayRegion(jenv, jarr, boff, blen, cbuf);
	if ((*jenv)->ExceptionCheck(jenv))
		return;
	ret = redfish_write(ofe, cbuf, blen);
	if (ret) {
		strerror_r(FORCE_POSITIVE(ret), err, err_len);
		redfish_throw(jenv, "java/io/IOException", err);
		goto done;
	}

done:
	if (blen > RF_DOUTSTREAM_MALLOC_THRESH)
		free(cbuf);
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishDataOutputStream_flush(
	JNIEnv *jenv, jobject jobj)
{
	int ret;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct redfish_file *ofe;

	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	ret = redfish_hflush(ofe);
	if (ret) {
		strerror_r(FORCE_POSITIVE(ret), err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
}

JNIEXPORT void JNICALL
Java_org_apache_hadoop_fs_redfish_RedfishDataOutputStream_sync(
	JNIEnv *jenv, jobject jobj)
{
	int ret;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct redfish_file *ofe;

	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	ret = redfish_hsync(ofe);
	if (ret) {
		strerror_r(FORCE_POSITIVE(ret), err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
}
