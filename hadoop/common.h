/*
 * Copyright 2012 the Redfish authors
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

#ifndef REDFISH_JNI_COMMON_DOT_H
#define REDFISH_JNI_COMMON_DOT_H

#include <jni.h>

/** Primitive Redfish Hadoop JNI debugging.  This is mostly for debugging
 * JNI_OnLoad issues */
#define HJNI_DEBUG(...) printf(__VA_ARGS__)

extern jfieldID g_fid_m_cli;
extern jfieldID g_fid_rf_in_stream_m_ofe;
extern jfieldID g_fid_rf_out_stream_m_ofe;

extern jclass g_cls_file_status;
extern jmethodID g_mid_file_status_ctor;

extern jclass g_cls_fs_perm;
extern jmethodID g_mid_fs_perm_ctor;

extern jclass g_cls_path;
extern jmethodID g_mid_path_ctor;

extern jclass g_cls_rf_in_stream;
extern jmethodID g_mid_rf_in_stream_ctor;

extern jclass g_cls_rf_out_stream;
extern jmethodID g_mid_rf_out_stream_ctor;

extern jclass g_cls_block_loc;
extern jmethodID g_mid_block_loc_ctor;

extern jclass g_cls_string;

/** Raise a Java exception from a JNI method
 *
 * @param jenv		The JNI environment
 * @param name		The name of the exception to raise
 * @param msg		The message to use when constructing the exception
 */
extern void redfish_throw(JNIEnv *jenv, const char *name, const char *msg);

/** Validate parameters for an array read/write operation
 *
 * @param jenv		The JNI environment
 * @param jarr		The array
 * @param boff		beginning offset into the array
 * @param blen		length of the array interval we plan to use
 *
 * @return		0 on success; error code otherwise.  A java exception
 *			will be raised on error.
 */
extern jint validate_rw_params(JNIEnv *jenv, jbyteArray jarr,
			jint boff, jint blen);

/** Translates a java string into a C string
 *
 * @param jenv		The JNI environment
 * @param jstr		The java string
 * @param cstr		The C string
 * @param cstr_len	Length of the C string
 *
 * @return		0 on success; error code otherwise.  A java exception
 *			will be raised on error.
 */
extern int jstr_to_cstr(JNIEnv *jenv, jstring jstr, char *cstr,
			size_t cstr_len);

#endif
