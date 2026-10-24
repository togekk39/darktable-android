/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <jni.h>
#include <stdint.h>
#include "dt_mobile.h"
static void throw_io(JNIEnv *env, const char *message)
{
  jclass type = (*env)->FindClass(env, "java/io/IOException");
  if(type) (*env)->ThrowNew(env, type, message);
}
static void throw_status(JNIEnv *env, dt_mobile_session *session, dt_mobile_status status)
{
  if(status == DT_MOBILE_OK) return;
  const char *error = dt_mobile_last_error(session);
  throw_io(env, status == DT_MOBILE_ERROR_INVALID_ARGUMENT ? "invalid native editor arguments" :
                (error && error[0] ? error : "native editor operation failed"));
}
JNIEXPORT jint JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_capabilities(JNIEnv *env, jobject self)
{ (void)env; (void)self; return (jint)dt_mobile_capabilities(); }
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_configureRuntime(JNIEnv *env, jobject self,
  jstring data_dir, jstring module_dir, jstring temp_root)
{
  (void)self;
  if(!data_dir || !module_dir || !temp_root) { throw_io(env, "runtime directories are required"); return; }
  const char *data = (*env)->GetStringUTFChars(env, data_dir, NULL);
  if(!data) return;
  const char *modules = (*env)->GetStringUTFChars(env, module_dir, NULL);
  if(!modules) { (*env)->ReleaseStringUTFChars(env, data_dir, data); return; }
  const char *temporary = (*env)->GetStringUTFChars(env, temp_root, NULL);
  if(!temporary) {
    (*env)->ReleaseStringUTFChars(env, module_dir, modules);
    (*env)->ReleaseStringUTFChars(env, data_dir, data); return;
  }
  const dt_mobile_status status = dt_mobile_configure_runtime(data, modules, temporary);
  (*env)->ReleaseStringUTFChars(env, temp_root, temporary);
  (*env)->ReleaseStringUTFChars(env, module_dir, modules);
  (*env)->ReleaseStringUTFChars(env, data_dir, data);
  if(status != DT_MOBILE_OK) throw_io(env, dt_mobile_runtime_error());
}
JNIEXPORT jlong JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_open(JNIEnv *env, jobject self, jstring path)
{
  (void)self; if(!path) { throw_io(env, "a source path is required"); return 0; }
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
  if(!rgba || width <= 0 || height <= 0 || width > max_width || height > max_height)
  { dt_mobile_free_buffer(rgba); throw_io(env, "native preview dimensions are invalid"); return NULL; }
  size_t size = (size_t)width * (size_t)height * 4u;
  if(size > 2147483647u) { dt_mobile_free_buffer(rgba); throw_io(env, "preview buffer is too large"); return NULL; }
  jbyteArray result = (*env)->NewByteArray(env, (jsize)size);
  if(result) (*env)->SetByteArrayRegion(env, result, 0, (jsize)size, (const jbyte *)rgba);
  dt_mobile_free_buffer(rgba);
  if(!result || (*env)->ExceptionCheck(env)) return NULL;
  jint values[2] = { width, height }; (*env)->SetIntArrayRegion(env, dimensions, 0, 2, values);
  return result;
}
static jstring metadata(JNIEnv *env, jlong handle, int formats)
{
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  char *json = NULL;
  dt_mobile_status status = formats ? dt_mobile_export_formats(session, &json) : dt_mobile_modules(session, &json);
  if(status != DT_MOBILE_OK) { dt_mobile_free_buffer(json); throw_status(env, session, status); return NULL; }
  if(!json) { throw_io(env, "native editor metadata is missing"); return NULL; }
  jstring result = (*env)->NewStringUTF(env, json);
  dt_mobile_free_buffer(json);
  return result;
}
JNIEXPORT jstring JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_listModules(JNIEnv *env, jobject self, jlong handle)
{ (void)self; return metadata(env, handle, 0); }
JNIEXPORT jstring JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_listExportFormats(JNIEnv *env, jobject self, jlong handle)
{ (void)self; return metadata(env, handle, 1); }
JNIEXPORT jstring JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_history(JNIEnv *env, jobject self, jlong handle)
{
  (void)self;
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  char *json = NULL;
  const dt_mobile_status status = dt_mobile_history(session, &json);
  if(status != DT_MOBILE_OK) { dt_mobile_free_buffer(json); throw_status(env, session, status); return NULL; }
  if(!json) { throw_io(env, "native edit history is missing"); return NULL; }
  jstring result = (*env)->NewStringUTF(env, json);
  dt_mobile_free_buffer(json);
  return result;
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_undo(JNIEnv *env, jobject self, jlong handle)
{
  (void)self;
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  throw_status(env, session, dt_mobile_undo(session));
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_redo(JNIEnv *env, jobject self, jlong handle)
{
  (void)self;
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  throw_status(env, session, dt_mobile_redo(session));
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_setParameter(JNIEnv *env, jobject self, jlong handle, jstring module, jstring parameter, jdouble value)
{
  (void)self;
  if(!module || !parameter) { throw_io(env, "module and parameter are required"); return; }
  const char *operation = (*env)->GetStringUTFChars(env, module, NULL);
  if(!operation) return;
  const char *name = (*env)->GetStringUTFChars(env, parameter, NULL);
  if(!name) { (*env)->ReleaseStringUTFChars(env, module, operation); return; }
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  dt_mobile_status status = dt_mobile_set_parameter(session, operation, name, value);
  (*env)->ReleaseStringUTFChars(env, parameter, name);
  (*env)->ReleaseStringUTFChars(env, module, operation);
  throw_status(env, session, status);
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_setModuleEnabled(JNIEnv *env, jobject self, jlong handle, jstring module, jboolean enabled)
{
  (void)self;
  if(!module) { throw_io(env, "a module is required"); return; }
  const char *operation = (*env)->GetStringUTFChars(env, module, NULL);
  if(!operation) return;
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  dt_mobile_status status = dt_mobile_set_enabled(session, operation, enabled == JNI_TRUE);
  (*env)->ReleaseStringUTFChars(env, module, operation);
  throw_status(env, session, status);
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_setCurve(JNIEnv *env, jobject self,
    jlong handle, jstring module, jint channel, jint interpolation, jfloatArray coordinates)
{
  (void)self;
  if(!module || !coordinates) { throw_io(env, "a curve module and coordinates are required"); return; }
  const jsize length = (*env)->GetArrayLength(env, coordinates);
  if(length < 4 || length > 40 || length % 2)
  { throw_io(env, "a curve needs 2 to 20 coordinate pairs"); return; }
  jfloat values[40];
  (*env)->GetFloatArrayRegion(env, coordinates, 0, length, values);
  if((*env)->ExceptionCheck(env)) return;
  const char *operation = (*env)->GetStringUTFChars(env, module, NULL);
  if(!operation) return;
  dt_mobile_curve_point points[20];
  const size_t count = (size_t)length / 2;
  for(size_t k = 0; k < count; k++) points[k] = (dt_mobile_curve_point) { values[2 * k], values[2 * k + 1] };
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  const dt_mobile_status status = dt_mobile_set_curve(session, operation, channel, interpolation, points, count);
  (*env)->ReleaseStringUTFChars(env, module, operation);
  throw_status(env, session, status);
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_setRgbCurveOptions(JNIEnv *env, jobject self,
    jlong handle, jstring module, jint mode, jint preserve_colors)
{
  (void)self;
  if(!handle || !module) { throw_io(env, "an image and RGB curve module are required"); return; }
  const char *operation = (*env)->GetStringUTFChars(env, module, NULL);
  if(!operation) return;
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  const dt_mobile_status status = dt_mobile_set_rgb_curve_options(session, operation, mode, preserve_colors);
  (*env)->ReleaseStringUTFChars(env, module, operation);
  throw_status(env, session, status);
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_setWhiteBalanceGains(JNIEnv *env, jobject self,
    jlong handle, jstring module, jfloatArray gains)
{
  (void)self;
  if(!handle || !module || !gains)
  { throw_io(env, "an image, white-balance module and complete gains are required"); return; }
  const jsize count = (*env)->GetArrayLength(env, gains);
  if(count != 3 && count != 4)
  { throw_io(env, "white balance requires three or four active channel gains"); return; }
  jfloat values[4];
  (*env)->GetFloatArrayRegion(env, gains, 0, count, values);
  if((*env)->ExceptionCheck(env)) return;
  const char *operation = (*env)->GetStringUTFChars(env, module, NULL);
  if(!operation) return;
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  const dt_mobile_status status = dt_mobile_set_whitebalance_gains(session, operation, values, (size_t)count);
  (*env)->ReleaseStringUTFChars(env, module, operation);
  throw_status(env, session, status);
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_setWhiteBalanceSetting(JNIEnv *env, jobject self,
    jlong handle, jstring module, jint setting)
{
  (void)self;
  if(!handle || !module) { throw_io(env, "an image and white-balance module are required"); return; }
  const char *operation = (*env)->GetStringUTFChars(env, module, NULL);
  if(!operation) return;
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  const dt_mobile_status status = dt_mobile_set_whitebalance_setting(session, operation, setting);
  (*env)->ReleaseStringUTFChars(env, module, operation);
  throw_status(env, session, status);
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_setCrop(JNIEnv *env, jobject self,
    jlong handle, jstring module, jfloat left, jfloat top, jfloat right, jfloat bottom)
{
  (void)self;
  if(!module) { throw_io(env, "a crop module is required"); return; }
  const char *operation = (*env)->GetStringUTFChars(env, module, NULL);
  if(!operation) return;
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  const dt_mobile_status status = dt_mobile_set_crop(session, operation, left, top, right, bottom);
  (*env)->ReleaseStringUTFChars(env, module, operation);
  throw_status(env, session, status);
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_exportImage(JNIEnv *env, jobject self, jlong handle, jstring path, jstring format, jint quality)
{
  (void)self;
  if(!path || !format) { throw_io(env, "output path and format are required"); return; }
  const char *output = (*env)->GetStringUTFChars(env, path, NULL);
  if(!output) return;
  const char *type = (*env)->GetStringUTFChars(env, format, NULL);
  if(!type) { (*env)->ReleaseStringUTFChars(env, path, output); return; }
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  dt_mobile_status status = dt_mobile_export(session, output, type, quality);
  (*env)->ReleaseStringUTFChars(env, format, type);
  (*env)->ReleaseStringUTFChars(env, path, output);
  throw_status(env, session, status);
}
JNIEXPORT void JNICALL Java_org_example_darktableandroid_nativecore_NativeCore_exportWithSettings(JNIEnv *env, jobject self,
    jlong handle, jstring path, jstring format, jintArray values)
{
  (void)self;
  if(!handle || !path || !format || !values || (*env)->GetArrayLength(env, values) != 12)
  { throw_io(env, "an image, output path, format and 12 export settings are required"); return; }
  jint fields[12];
  (*env)->GetIntArrayRegion(env, values, 0, 12, fields);
  if((*env)->ExceptionCheck(env)) return;
  /* Reject signed Java dimensions before converting to the public uint32_t
   * fields. All remaining ranges and format constraints use native validation. */
  if(fields[0] < 0 || fields[1] < 0)
  { throw_io(env, "export dimensions must be nonnegative"); return; }
  const dt_mobile_export_settings settings = {
    .struct_size = sizeof(dt_mobile_export_settings),
    .max_width = (uint32_t)fields[0], .max_height = (uint32_t)fields[1],
    .quality = fields[2], .pixel_format = fields[3],
    .png_compression = fields[4], .tiff_compression = fields[5],
    .tiff_compression_level = fields[6], .jpeg_subsample = fields[7],
    .dpi = fields[8], .output_profile = fields[9],
    .rendering_intent = fields[10], .metadata_policy = fields[11]
  };
  const char *output = (*env)->GetStringUTFChars(env, path, NULL);
  if(!output) return;
  const char *type = (*env)->GetStringUTFChars(env, format, NULL);
  if(!type) { (*env)->ReleaseStringUTFChars(env, path, output); return; }
  dt_mobile_session *session = (dt_mobile_session *)(intptr_t)handle;
  const dt_mobile_status status = dt_mobile_export_with_settings(session, output, type, &settings);
  (*env)->ReleaseStringUTFChars(env, format, type);
  (*env)->ReleaseStringUTFChars(env, path, output);
  throw_status(env, session, status);
}
