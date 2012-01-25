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
#include <stdlib.h>
#include <string.h>

#include "client/fishc.h"
#include "hadoop/common.h"
#include "mds/limits.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/macro.h"

#define RF_DINSTREAM_MALLOC_THRESH 1024

BUILD_BUG_ON(sizeof(jlong) < sizeof(void*));
BUILD_BUG_ON(sizeof(jlong) != sizeof(int64_t));
BUILD_BUG_ON(sizeof(jint) != sizeof(int32_t));

static void redfish_set_m_ofe(JNIEnv *jenv, jobject jobj, void *ptr)
{
	(*jenv)->SetLongField(jenv, jobj, g_fid_rf_in_stream_m_ofe,
			(jlong)(uintptr_t)ptr);
}

static void* redfish_get_m_ofe(JNIEnv *jenv, jobject jobj)
{
	return (void*)(uintptr_t)(*jenv)->GetLongField(jenv, jobj,
			g_fid_rf_in_stream_m_ofe);
}

jint Java_org_apache_hadoop_fs_redfish_RedfishDataInputStream_available(
		JNIEnv *jenv, jobject jobj)
{
	int32_t res = -EINVAL;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct redfish_file *ofe;

	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	res = redfish_available(ofe);
	if (res < 0) {
		strerror_r(res, err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return res;
}

static jint validate_read_params(JNIEnv *jenv, jbyteArray jarr,
		jint boff, jint blen)
{
	int32_t alen;
	uint32_t end;

	if (boff < 0) {
		redfish_throw(jenv, "java/lang/IndexOutOfBoundsException",
				"boff < 0");
		return -1;
	}
	if (blen < 0) {
		redfish_throw(jenv, "java/lang/IndexOutOfBoundsException",
				"blen < 0");
		return -1;
	}
	if (jarr == NULL) {
		redfish_throw(jenv, "java/lang/NullPointerException",
				"buf == NULL");
		return -1;
	}
	/* It's important to do the addition of boff and blen as an unsigned
	 * operation, so that we don't get undefined behavior on integer
	 * overflow.  We do the comparison as a signed comparison, so that if
	 * overflow did take place, we're comparing a positive number with a
	 * negative one.
	 *
	 * Unlike C, Java defines integers as 4 bytes, no matter what the
	 * underlying machine architecture may be.  That's why we can ignore the
	 * jint, etc typedefs and just use uint32_t and friends.
	 */
	alen = (*jenv)->GetArrayLength(jenv, jarr);
	end = ((uint32_t)boff) + ((uint32_t)blen);
	if (((int32_t)end) < alen) {
		redfish_throw(jenv, "java/lang/IndexOutOfBoundsException",
				"boff + blen > buf.length()");
		return -1;
	}
	return 0;
}

jint redfishDoRead(JNIEnv *jenv, jobject jobj, jlong jpos, jbyteArray jarr,
		jint boff, jint blen)
{
	jint ret = -1;
	int8_t *cbuf = NULL;
	int8_t stack_buf[(blen < RF_DINSTREAM_MALLOC_THRESH) ? blen : 1];
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct redfish_file *ofe;

	ret = validate_read_params(jenv, jarr, boff, blen);
	if (ret)
		return ret;
	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe) {
		redfish_throw(jenv, "java/io/IOException", "m_ofe == NULL");
		return -1;
	}
	if (blen >= RF_DINSTREAM_MALLOC_THRESH) {
		cbuf = malloc(blen);
		if (!cbuf) {
			strerror_r(ENOMEM, err, err_len);
			redfish_throw(jenv, "java/io/IOException", err);
			return -1;
		}
	}
	else {
		cbuf = stack_buf;
	}
	if (jpos < 0) {
		ret = redfish_read(ofe, cbuf, blen);
	}
	else {
		ret = redfish_pread(ofe, cbuf, blen, jpos);
	}
	if (ret < 0) {
		strerror_r(FORCE_POSITIVE(ret), err, err_len);
		goto done;
	}
	(*jenv)->SetByteArrayRegion(jenv, jarr, boff, blen, cbuf);

done:
	if (blen >= RF_DINSTREAM_MALLOC_THRESH)
		free(cbuf);
	return ret;
}

jint Java_org_apache_hadoop_fs_redfish_RedfishDataInputStream_redfishRead(
		JNIEnv *jenv, jobject jobj, jbyteArray jarr, jint boff, jint blen)
{
	return redfishDoRead(jenv, jobj, -1, jarr, boff, blen);
}

jint Java_org_apache_hadoop_fs_redfish_RedfishDataInputStream_redfishPread(
		JNIEnv *jenv, jobject jobj, jlong jpos, jbyteArray jarr,
		jint boff, jint blen)
{
	if (jpos < 0) {
		redfish_throw(jenv, "java/io/IOException", "jpos < 0");
		return -1;
	}
	return redfishDoRead(jenv, jobj, jpos, jarr, boff, blen);
}

void Java_org_apache_hadoop_fs_redfish_RedfishDataInputStream_redfishFlush(
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

void Java_org_apache_hadoop_fs_redfish_RedfishDataInputStream_redfishClose(
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
	ret = redfish_close(ofe);
	if (ret) {
		strerror_r(FORCE_POSITIVE(ret), err, err_len);
		goto done;
	}
done:
	redfish_set_m_ofe(jenv, jobj, NULL);
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
}

void Java_org_apache_hadoop_fs_redfish_RedfishDataInputStream_redfishFree(
		JNIEnv *jenv, jobject jobj)
{
	struct redfish_file *ofe;

	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe)
		return;
	redfish_free_file(ofe);
	redfish_set_m_ofe(jenv, jobj, NULL);
}

void Java_org_apache_hadoop_fs_redfish_RedfishDataInputStream_seek(
		JNIEnv *jenv, jobject jobj, jlong off)
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
	ret = redfish_fseek_abs(ofe, off);
	if (ret) {
		strerror_r(FORCE_POSITIVE(ret), err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
}

jlong Java_org_apache_hadoop_fs_redfish_RedfishDataInputStream_skip(
		JNIEnv *jenv, jobject jobj, jlong delta)
{
	int ret;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct redfish_file *ofe;
	int64_t out = 0;

	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	ret = redfish_fseek_rel(ofe, delta, &out);
	if (ret) {
		strerror_r(FORCE_POSITIVE(ret), err, err_len);
		goto done;
	}
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return out;
}

jlong Java_org_apache_hadoop_fs_redfish_RedfishDataInputStream_getPos(
		JNIEnv *jenv, jobject jobj)
{
	int64_t ret = 0;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct redfish_file *ofe;

	ofe = redfish_get_m_ofe(jenv, jobj);
	if (!ofe) {
		strerror_r(EINVAL, err, err_len);
		goto done;
	}
	ret = redfish_ftell(ofe);
done:
	if (err[0])
		redfish_throw(jenv, "java/io/IOException", err);
	return ret;
}
