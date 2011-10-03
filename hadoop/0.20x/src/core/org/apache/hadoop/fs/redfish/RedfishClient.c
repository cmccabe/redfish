/**
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

#include <jni.h>

#include "fishc.h"

static void set_ptr(JNIEnv *jenv, jobject jobj, const char *name, void *ptr)
{
	BUILD_BUG_ON(sizeof(jlong) < sizeof(void*));

	jclass jcls = (*jenv)->GetObjectClass(jobj);
	jfieldID jfid = (*jenv)->GetFieldID(jcls, name, "Ljava/lang/Long;");
	if (jfid == NULL)
		return;
	(*jenv)->SetLongField(obj, jfid, (jlong)(uintptr_t)ptr);
}

static void* get_ptr(JNIEnv *jenv, jobject jobj, const char *name)
{
	BUILD_BUG_ON(sizeof(jlong) < sizeof(void*));

	jclass cls = (*jenv)->GetObjectClass(jobj);
	jfieldID jfid = (*jenv)->GetFieldID(cls, "cmount", "Ljava/lang/Long;");
	if (jfid == NULL)
		return NULL;
	return (void*)(uintptr_t)(*jenv)->GetLongField(obj, jfid);
}

void Java_org_apache_hadoop_fs_redfish_RedfishClient_redfishConnect(
		JNIEnv *jenv, jobject jobj, jstring jhost, jint jport, jstring juser)
{
	int ret;
	const char *chost = NULL, *cuser = NULL;
	struct of_client *cli  = NULL;
	
	cli = get_ptr(jenv, jobj, "m_cli");
	if (cli != NULL) {
		/* already initialized */
		... throw exception ...
	}

	chost = (*jenv)->GetStringUTFChars(jhost, 0);
	if (!chost) {
		ret = -ENOMEM;
		goto error;
	}
	cuser = (*jenv)->GetStringUTFChars(juser, 0);
	if (!cuser) {
		ret = -ENOMEM;
		goto error;
	}

	... set up mlocs ...
	ret = onefish_connect(mlocs, cuser, &cli);
	if (ret)
		goto done;
	set_ptr(jenv, jobj, "m_cli", cli);
done:
	if (chost)
		(*jenv)->ReleaseStringUTFChars(jhost, chost);
	if (cuser)
		(*jenv)->ReleaseStringUTFChars(juser, cuser);
	if (ret) {
		... throw exception ...
	}
}

...
