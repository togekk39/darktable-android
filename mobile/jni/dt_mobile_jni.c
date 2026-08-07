/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <jni.h>
#include <stdint.h>
#include "dt_mobile.h"
JNIEXPORT jlong JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_open(JNIEnv *env, jobject self, jstring path)
{
  (void)self; if(!path) return 0;
  const char *value = (*env)->GetStringUTFChars(env, path, NULL); if(!value) return 0;
  dt_mobile_session *session = NULL; dt_mobile_status status = dt_mobile_open(value, &session);
  (*env)->ReleaseStringUTFChars(env, path, value);
  return status == DT_MOBILE_OK ? (jlong)(intptr_t)session : 0;
}
JNIEXPORT jstring JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_lastError(JNIEnv *env, jobject self, jlong handle)
{ (void)self; return (*env)->NewStringUTF(env, dt_mobile_last_error((dt_mobile_session *)(intptr_t)handle)); }
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_cancel(JNIEnv *env, jobject self, jlong handle)
{ (void)env; (void)self; dt_mobile_cancel((dt_mobile_session *)(intptr_t)handle); }
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_close(JNIEnv *env, jobject self, jlong handle)
{ (void)env; (void)self; dt_mobile_close((dt_mobile_session *)(intptr_t)handle); }
