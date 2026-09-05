/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "darktable_mobile_engine.h"
#include "common/darktable.h"
#include "common/film.h"
#include "common/image.h"
#include "imageio/imageio_common.h"
#include <glib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

struct dt_mobile_engine { dt_imgid_t image_id; atomic_bool cancelled; pthread_mutex_t operation_lock;
  dt_imageio_preview_cancel_t *preview_cancel; };
static pthread_mutex_t runtime_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t engine_work_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned runtime_users;
static char *runtime_datadir;
static char *runtime_moduledir;

static dt_mobile_status engine_fail(char *error, size_t size, dt_mobile_status status, const char *text)
{ if(error && size) snprintf(error, size, "%s", text); return status; }

static gboolean is_directory(const char *path)
{
  struct stat info;
  return path && path[0] && !stat(path, &info) && S_ISDIR(info.st_mode);
}

dt_mobile_status dt_mobile_engine_initialize(const char *datadir, const char *moduledir)
{
  pthread_mutex_lock(&runtime_lock);
  if(runtime_users) { pthread_mutex_unlock(&runtime_lock); return DT_MOBILE_ERROR_ENGINE; }
  if(!is_directory(datadir) || !is_directory(moduledir))
  { pthread_mutex_unlock(&runtime_lock); return DT_MOBILE_ERROR_INVALID_ARGUMENT; }
  char *data = g_strdup(datadir), *modules = g_strdup(moduledir);
  if(!data || !modules) { g_free(data); g_free(modules); pthread_mutex_unlock(&runtime_lock); return DT_MOBILE_ERROR_OUT_OF_MEMORY; }
  g_free(runtime_datadir); g_free(runtime_moduledir);
  runtime_datadir = data; runtime_moduledir = modules;
  pthread_mutex_unlock(&runtime_lock);
  return DT_MOBILE_OK;
}

dt_mobile_status dt_mobile_engine_open(const char *path, dt_mobile_engine **out,
                                        char *error, size_t error_size)
{
  *out = NULL;
  pthread_mutex_lock(&runtime_lock);
  if(!runtime_users)
  {
    const char *datadir = runtime_datadir;
    const char *moduledir = runtime_moduledir;
    if(!is_directory(datadir) || !is_directory(moduledir))
    {
      pthread_mutex_unlock(&runtime_lock);
      return engine_fail(error, error_size, DT_MOBILE_ERROR_ENGINE,
                         "darktable runtime paths were not initialized by the Android application");
    }
    char *argv[] = { (char *)"darktable-mobile", (char *)"--library", (char *)":memory:",
                     (char *)"--datadir", (char *)datadir,
                     (char *)"--moduledir", (char *)moduledir, NULL };
    if(dt_init(7, argv, FALSE, TRUE, NULL))
    { pthread_mutex_unlock(&runtime_lock); return engine_fail(error, error_size, DT_MOBILE_ERROR_ENGINE, "darktable headless initialization failed"); }
  }
  runtime_users++;
  pthread_mutex_unlock(&runtime_lock);

  gchar *directory = g_path_get_dirname(path);
  dt_film_t film;
  const dt_filmid_t film_id = dt_film_new(&film, directory);
  g_free(directory);
  const dt_imgid_t image_id = dt_is_valid_filmid(film_id)
    ? dt_image_import(film_id, path, TRUE, FALSE) : NO_IMGID;
  if(!dt_is_valid_imgid(image_id))
  {
    pthread_mutex_lock(&runtime_lock); runtime_users--; if(!runtime_users) dt_cleanup(); pthread_mutex_unlock(&runtime_lock);
    return engine_fail(error, error_size, DT_MOBILE_ERROR_DECODE, "darktable could not import or decode the image");
  }
  dt_mobile_engine *engine = calloc(1, sizeof(*engine));
  if(!engine)
  {
    pthread_mutex_lock(&runtime_lock); runtime_users--; if(!runtime_users) dt_cleanup(); pthread_mutex_unlock(&runtime_lock);
    return engine_fail(error, error_size, DT_MOBILE_ERROR_OUT_OF_MEMORY, "unable to allocate darktable image context");
  }
  engine->image_id = image_id;
  atomic_init(&engine->cancelled, false);
  pthread_mutex_init(&engine->operation_lock, NULL);
  engine->preview_cancel = dt_imageio_preview_cancel_new();
  *out = engine;
  return DT_MOBILE_OK;
}

dt_mobile_status dt_mobile_engine_render(dt_mobile_engine *engine, int mw, int mh,
                                          uint8_t **pixels, int *width, int *height,
                                          char *error, size_t error_size)
{
  pthread_mutex_lock(&engine->operation_lock);
  if(atomic_load(&engine->cancelled))
  {
    pthread_mutex_unlock(&engine->operation_lock);
    return engine_fail(error, error_size, DT_MOBILE_ERROR_CANCELLED, "operation cancelled");
  }
  uint32_t w = 0, h = 0;
  pthread_mutex_lock(&engine_work_lock);
  if(dt_imageio_preview_to_memory(engine->image_id, (size_t)mw, (size_t)mh, pixels, &w, &h,
                                  engine->preview_cancel))
  {
    pthread_mutex_unlock(&engine_work_lock);
    pthread_mutex_unlock(&engine->operation_lock);
    if(atomic_load(&engine->cancelled))
      return engine_fail(error, error_size, DT_MOBILE_ERROR_CANCELLED, "operation cancelled");
    return engine_fail(error, error_size, DT_MOBILE_ERROR_PROCESSING, "darktable preview pixelpipe failed");
  }
  pthread_mutex_unlock(&engine_work_lock);
  if(atomic_load(&engine->cancelled)) { free(*pixels); *pixels = NULL; pthread_mutex_unlock(&engine->operation_lock); return engine_fail(error, error_size, DT_MOBILE_ERROR_CANCELLED, "operation cancelled"); }
  *width = (int)w; *height = (int)h;
  pthread_mutex_unlock(&engine->operation_lock);
  return DT_MOBILE_OK;
}
void dt_mobile_engine_cancel(dt_mobile_engine *engine) { if(engine) { atomic_store(&engine->cancelled, true);
  dt_imageio_preview_cancel(engine->preview_cancel); } }
void dt_mobile_engine_close(dt_mobile_engine *engine)
{
  if(!engine) return;
  pthread_mutex_lock(&engine->operation_lock);
  pthread_mutex_unlock(&engine->operation_lock);
  pthread_mutex_destroy(&engine->operation_lock);
  dt_imageio_preview_cancel_free(engine->preview_cancel);
  free(engine);
  pthread_mutex_lock(&runtime_lock);
  if(runtime_users && !--runtime_users) dt_cleanup();
  pthread_mutex_unlock(&runtime_lock);
}
