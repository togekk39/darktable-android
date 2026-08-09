/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dt_mobile.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct dt_mobile_session { char *path; atomic_bool cancelled; char error[256]; };
static _Thread_local char open_error[256];
static dt_mobile_status fail(dt_mobile_session *s, dt_mobile_status status, const char *message)
{ if(s) snprintf(s->error, sizeof(s->error), "%s", message); return status; }
dt_mobile_status dt_mobile_open(const char *path, dt_mobile_session **out)
{
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
  strcpy(s->path, path); atomic_init(&s->cancelled, 0); *out = s; return DT_MOBILE_OK;
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
  return fail(s, DT_MOBILE_ERROR_UNSUPPORTED, "darktable RAW loader/pixelpipe integration is pending");
}
dt_mobile_status dt_mobile_export(dt_mobile_session *s, const char *path, const char *format, int quality)
{
  if(!s || !path || !format || quality < 0 || quality > 100) return DT_MOBILE_ERROR_INVALID_ARGUMENT;
  if(atomic_load(&s->cancelled)) return fail(s, DT_MOBILE_ERROR_CANCELLED, "operation cancelled");
  return fail(s, DT_MOBILE_ERROR_UNSUPPORTED, "darktable export adapter is not linked yet");
}
void dt_mobile_free_buffer(void *buffer) { free(buffer); }
void dt_mobile_cancel(dt_mobile_session *s) { if(s) atomic_store(&s->cancelled, 1); }
const char *dt_mobile_last_error(const dt_mobile_session *s) { return s ? s->error : "invalid session"; }
void dt_mobile_close(dt_mobile_session *s) { if(s) { free(s->path); free(s); } }
