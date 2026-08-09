/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <jni.h>
#include <stdint.h>
#include "dt_mobile.h"
static void throw_io(JNIEnv *env, const char *message)
{
  jclass type = (*env)->FindClass(env, "java/io/IOException");
  if(type) (*env)->ThrowNew(env, type, message);
}
JNIEXPORT jlong JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_open(JNIEnv *env, jobject self, jstring path)
{
  (void)self; if(!path) return 0;
  const char *value = (*env)->GetStringUTFChars(env, path, NULL); if(!value) return 0;
  dt_mobile_session *session = NULL; dt_mobile_status status = dt_mobile_open(value, &session);
  (*env)->ReleaseStringUTFChars(env, path, value);
  if(status != DT_MOBILE_OK) { throw_io(env, dt_mobile_open_error()); return 0; }
  return (jlong)(intptr_t)session;
}
JNIEXPORT jstring JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_lastError(JNIEnv *env, jobject self, jlong handle)
{ (void)self; return (*env)->NewStringUTF(env, dt_mobile_last_error((dt_mobile_session *)(intptr_t)handle)); }
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_cancel(JNIEnv *env, jobject self, jlong handle)
{ (void)env; (void)self; dt_mobile_cancel((dt_mobile_session *)(intptr_t)handle); }
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_close(JNIEnv *env, jobject self, jlong handle)
{ (void)env; (void)self; dt_mobile_close((dt_mobile_session *)(intptr_t)handle); }
JNIEXPORT jbyteArray JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_renderPreview(JNIEnv *env, jobject self, jlong handle, jint max_width, jint max_height, jintArray dimensions)
{
  (void)self;
  if(!handle || !dimensions || (*env)->GetArrayLength(env, dimensions) < 2) { throw_io(env, "invalid preview arguments"); return NULL; }
  uint8_t *rgba = NULL; int width = 0, height = 0;
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  dt_mobile_status status = dt_mobile_render_preview(session, max_width, max_height, &rgba, &width, &height);
  if(status != DT_MOBILE_OK) { throw_io(env, dt_mobile_last_error(session)); return NULL; }
  size_t size = (size_t)width * (size_t)height * 4u;
  if(size > 2147483647u) { dt_mobile_free_buffer(rgba); throw_io(env, "preview buffer is too large"); return NULL; }
  jbyteArray result = (*env)->NewByteArray(env, (jsize)size);
  if(result) (*env)->SetByteArrayRegion(env, result, 0, (jsize)size, (const jbyte *)rgba);
  dt_mobile_free_buffer(rgba);
  if(!result || (*env)->ExceptionCheck(env)) return NULL;
  jint values[2] = { width, height }; (*env)->SetIntArrayRegion(env, dimensions, 0, 2, values);
  return result;
}
