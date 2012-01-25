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
#include <stdint.h>

#include "hadoop/common.h"
#include "util/compiler.h"

jfieldID g_fid_m_cli;
jfieldID g_fid_rf_in_stream_m_ofe;
jfieldID g_fid_rf_out_stream_m_ofe;

jclass g_cls_file_status;
jmethodID g_mid_file_status_ctor;

jclass g_cls_file_perm;
jmethodID g_mid_file_perm_ctor;

jclass g_cls_path;
jmethodID g_mid_path_ctor;

jclass g_cls_rf_in_stream;
jmethodID g_mid_rf_in_stream_ctor;

jclass g_cls_rf_out_stream;
jmethodID g_mid_rf_out_stream_ctor;

static int cache_class_and_ctor(JNIEnv *jenv, const char *name,
		jclass *out_cls, jmethodID *out_ctor, const char *sig)
{
	jclass cls;
	jmethodID ctor;

	cls = (*jenv)->FindClass(jenv, name);
	if (!cls)
		return -ENOENT;
	ctor = (*jenv)->GetMethodID(jenv, cls, "<init>", sig);
	if (!ctor)
		return -ENOENT;
	*out_cls = (*jenv)->NewWeakGlobalRef(jenv, cls);
	if (!(*out_cls))
		return -ENOENT;
	*out_ctor = ctor;
	return 0;
}

static void uncache_class_and_ctor(JNIEnv *jenv,
		jclass *out_cls, jmethodID *out_ctor)
{
	(*jenv)->DeleteGlobalRef(jenv, *out_cls);
	*out_cls = NULL;
	*out_ctor = NULL;
}

static int cache_redfish_client_fields(JNIEnv *jenv)
{
	jclass cls;

	cls = (*jenv)->FindClass(jenv, "RedfishClient");
	if (!cls)
		return -ENOENT;
	g_fid_m_cli = (*jenv)->GetFieldID(jenv, cls,
			"m_cli", "Ljava/lang/Long;");
	if (!g_fid_m_cli)
		return -ENOENT;
	return 0;
}

void redfish_throw(JNIEnv *jenv, const char *name, const char *msg)
{
	jclass cls = (*jenv)->FindClass(jenv, name);
	if (!cls) {
		/* If !cls, we just raised a NoClassDefFound exception, or
		 * similar. */
		return;
	}
	(*jenv)->ThrowNew(jenv, cls, msg);
	(*jenv)->DeleteLocalRef(jenv, cls);
}

static int cache_redfish_input_stream_fields(JNIEnv *jenv)
{
	int ret;

	ret = cache_class_and_ctor(jenv, "RedfishDataInputStream",
		&g_cls_rf_in_stream, &g_mid_rf_in_stream_ctor, "(J)V");
	if (ret)
		return -ENOENT;
	g_fid_rf_in_stream_m_ofe = (*jenv)->GetFieldID(jenv, g_cls_rf_in_stream,
			"m_ofe", "Ljava/lang/Long;");
	if (!g_fid_rf_in_stream_m_ofe)
		return -ENOENT;
	return 0;
}

static int cache_redfish_output_stream_fields(JNIEnv *jenv)
{
	int ret;

	ret = cache_class_and_ctor(jenv, "RedfishDataOutputStream",
		&g_cls_rf_out_stream, &g_mid_rf_out_stream_ctor, "(J)V");
	if (ret)
		return -ENOENT;
	g_fid_rf_out_stream_m_ofe = (*jenv)->GetFieldID(jenv, g_cls_rf_out_stream,
			"m_ofe", "Ljava/lang/Long;");
	if (!g_fid_rf_out_stream_m_ofe)
		return -ENOENT;
	return 0;
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *jvm, POSSIBLY_UNUSED(void *reserved))
{
	int ret;
	JNIEnv *jenv = NULL;

	if ((*jvm)->GetEnv(jvm, (void **)&jenv, JNI_VERSION_1_2)) {
		return JNI_ERR; /* JNI version not supported */
	}
	ret = cache_redfish_client_fields(jenv);
	if (ret)
		return JNI_ERR;
	ret = cache_redfish_input_stream_fields(jenv);
	if (ret)
		return JNI_ERR;
	ret = cache_redfish_output_stream_fields(jenv);
	if (ret)
		return JNI_ERR;
	// org.apache.hadoop.fs.FileStatus FileStatus
	ret = cache_class_and_ctor(jenv, "FileStatus", &g_cls_file_status,
			&g_mid_file_status_ctor,
			"(JZIJJJFsPermission;java/lang/String;java/lang/String;Path;)V");
	if (ret)
		return JNI_ERR;
	ret = cache_class_and_ctor(jenv, "FilePermission", &g_cls_file_perm,
			&g_mid_file_perm_ctor, "(S)V");
	if (ret)
		return JNI_ERR;
	ret = cache_class_and_ctor(jenv, "Path", &g_cls_path,
			&g_mid_path_ctor, "(java/lang/String;)V");
	if (ret)
		return JNI_ERR;
	return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM *jvm, POSSIBLY_UNUSED(void *reserved))
{
	JNIEnv *jenv;

	if ((*jvm)->GetEnv(jvm, (void **)&jenv, JNI_VERSION_1_2)) {
		return;
	}
	g_fid_m_cli = 0;
	uncache_class_and_ctor(jenv, &g_cls_file_status,
			&g_mid_file_status_ctor);
	uncache_class_and_ctor(jenv, &g_cls_file_perm,
			&g_mid_file_perm_ctor);
	uncache_class_and_ctor(jenv, &g_cls_path,
			&g_mid_path_ctor);
}

jint validate_rw_params(JNIEnv *jenv, jbyteArray jarr,
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
