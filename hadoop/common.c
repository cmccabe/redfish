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

#include "hadoop/common.h"
#include "util/compiler.h"

jfieldID g_fid_m_cli;

jclass g_cls_file_status;
jmethodID g_mid_file_status_ctor;

jclass g_cls_file_perm;
jmethodID g_mid_file_perm_ctor;

jclass g_cls_path;
jmethodID g_mid_path_ctor;

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

jint JNI_OnLoad(JavaVM *jvm, POSSIBLY_UNUSED(void *reserved))
{
	int ret;
	JNIEnv *jenv = NULL;

	if ((*jvm)->GetEnv(jvm, (void **)&jenv, JNI_VERSION_1_2)) {
		return JNI_ERR; /* JNI version not supported */
	}
	ret = cache_redfish_client_fields(jenv);
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

void JNI_OnUnload(JavaVM *jvm, POSSIBLY_UNUSED(void *reserved))
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
