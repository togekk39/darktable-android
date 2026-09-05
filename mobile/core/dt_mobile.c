/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dt_mobile.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DT_MOBILE_HAS_DARKTABLE_ENGINE
#include "darktable_mobile_engine.h"
#endif
struct dt_mobile_session {
  char *path;
  atomic_bool cancelled;
#ifdef DT_MOBILE_HAS_DARKTABLE_ENGINE
  dt_mobile_engine *engine;
#endif
  char error[256];
};
static _Thread_local char open_error[256];
#ifdef DT_MOBILE_HAS_DARKTABLE_ENGINE
static char *runtime_datadir;
static char *runtime_moduledir;
#endif
#ifdef DT_MOBILE_HAS_ANDROID_DEPENDENCIES
extern unsigned long dt_mobile_dependency_probe(void);
#endif
static dt_mobile_status fail(dt_mobile_session *s, dt_mobile_status status, const char *message)
{ if(s) snprintf(s->error, sizeof(s->error), "%s", message); return status; }
dt_mobile_status dt_mobile_initialize(const char *datadir, const char *moduledir)
{
#ifdef DT_MOBILE_HAS_DARKTABLE_ENGINE
  if(!datadir || !datadir[0] || !moduledir || !moduledir[0]) return DT_MOBILE_ERROR_INVALID_ARGUMENT;
  char *new_datadir = malloc(strlen(datadir) + 1), *new_moduledir = malloc(strlen(moduledir) + 1);
  if(!new_datadir || !new_moduledir) { free(new_datadir); free(new_moduledir); return DT_MOBILE_ERROR_OUT_OF_MEMORY; }
  strcpy(new_datadir, datadir); strcpy(new_moduledir, moduledir);
  free(runtime_datadir); free(runtime_moduledir);
  runtime_datadir = new_datadir; runtime_moduledir = new_moduledir;
  return dt_mobile_engine_initialize(runtime_datadir, runtime_moduledir);
#else
  (void)datadir; (void)moduledir;
  return DT_MOBILE_ERROR_UNSUPPORTED;
#endif
}
dt_mobile_status dt_mobile_open(const char *path, dt_mobile_session **out)
{
#ifdef DT_MOBILE_HAS_ANDROID_DEPENDENCIES
  (void)dt_mobile_dependency_probe();
#endif
  open_error[0] = '\0';
  if(!path || !path[0] || !out) { snprintf(open_error, sizeof(open_error), "a non-empty source path and output session are required"); return DT_MOBILE_ERROR_INVALID_ARGUMENT; }
  *out = NULL;
  FILE *input = fopen(path, "rb");
  if(!input) { snprintf(open_error, sizeof(open_error), "unable to open cached source: %s", path); return DT_MOBILE_ERROR_IO; }
  if(fseek(input, 0, SEEK_END) != 0 || ftell(input) < 16) { fclose(input); snprintf(open_error, sizeof(open_error), "source is empty or truncated"); return DT_MOBILE_ERROR_IO; }
  fclose(input);
  dt_mobile_session *s = calloc(1, sizeof(*s));
  if(!s) { snprintf(open_error, sizeof(open_error), "unable to allocate native session"); return DT_MOBILE_ERROR_OUT_OF_MEMORY; }
  s->path = malloc(strlen(path) + 1);
  if(!s->path) { free(s); return DT_MOBILE_ERROR_OUT_OF_MEMORY; }
  strcpy(s->path, path); atomic_init(&s->cancelled, 0);
#ifdef DT_MOBILE_HAS_DARKTABLE_ENGINE
  const dt_mobile_status status = dt_mobile_engine_open(path, &s->engine, s->error, sizeof(s->error));
  if(status != DT_MOBILE_OK)
  {
    snprintf(open_error, sizeof(open_error), "%s", s->error);
    free(s->path); free(s); return status;
  }
#endif
  *out = s; return DT_MOBILE_OK;
}
const char *dt_mobile_open_error(void) { return open_error[0] ? open_error : "native open failed"; }
dt_mobile_status dt_mobile_set_module_params(dt_mobile_session *s, const char *module,
                                               const void *params, size_t size)
{
  if(!s || !module || !params || !size) return DT_MOBILE_ERROR_INVALID_ARGUMENT;
  return fail(s, DT_MOBILE_ERROR_UNSUPPORTED, "darktable pixelpipe adapter is not linked yet");
}
dt_mobile_status dt_mobile_render_preview(dt_mobile_session *s, int mw, int mh,
                                            uint8_t **rgba, int *w, int *h)
{
  if(!s || mw <= 0 || mh <= 0 || !rgba || !w || !h) return DT_MOBILE_ERROR_INVALID_ARGUMENT;
  *rgba = NULL; *w = *h = 0;
  if(atomic_load(&s->cancelled)) return fail(s, DT_MOBILE_ERROR_CANCELLED, "operation cancelled");
#ifdef DT_MOBILE_HAS_DARKTABLE_ENGINE
  return dt_mobile_engine_render(s->engine, mw, mh, rgba, w, h, s->error, sizeof(s->error));
#else
  return fail(s, DT_MOBILE_ERROR_UNSUPPORTED, "this build does not contain the darktable engine");
#endif
}
dt_mobile_status dt_mobile_export(dt_mobile_session *s, const char *path, const char *format, int quality)
{
  if(!s || !path || !format || quality < 0 || quality > 100) return DT_MOBILE_ERROR_INVALID_ARGUMENT;
  if(atomic_load(&s->cancelled)) return fail(s, DT_MOBILE_ERROR_CANCELLED, "operation cancelled");
  return fail(s, DT_MOBILE_ERROR_UNSUPPORTED, "darktable export adapter is not linked yet");
}
void dt_mobile_free_buffer(void *buffer) { free(buffer); }
void dt_mobile_cancel(dt_mobile_session *s) { if(s) { atomic_store(&s->cancelled, 1);
#ifdef DT_MOBILE_HAS_DARKTABLE_ENGINE
  dt_mobile_engine_cancel(s->engine);
#endif
} }
const char *dt_mobile_last_error(const dt_mobile_session *s) { return s ? s->error : "invalid session"; }
void dt_mobile_close(dt_mobile_session *s) { if(s) {
#ifdef DT_MOBILE_HAS_DARKTABLE_ENGINE
  dt_mobile_engine_close(s->engine);
#endif
  free(s->path); free(s); } }
