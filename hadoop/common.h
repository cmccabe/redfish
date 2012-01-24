/*
 * Copyright 2012 the RedFish authors
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

extern jfieldID g_fid_m_cli;

extern jclass g_cls_file_status;
extern jmethodID g_mid_file_status_ctor;

extern jclass g_cls_file_perm;
extern jmethodID g_mid_file_perm_ctor;

extern jclass g_cls_path;
extern jmethodID g_mid_path_ctor;

/** Raise a Java exception from a JNI method
 *
 * @param jenv		The JNI environment
 * @param name		The name of the exception to raise
 * @param msg		The message to use when constructing the exception
 */
extern void redfish_throw(JNIEnv *jenv, const char *name, const char *msg);

#endif
