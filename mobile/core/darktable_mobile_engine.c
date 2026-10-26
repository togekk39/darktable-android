/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "darktable_mobile_engine.h"
#include "common/darktable.h"
#include "common/film.h"
#include "common/image.h"
#include "common/history.h"
#include "common/iop_order.h"
#include "common/whitebalance.h"
#include "control/conf.h"
#include "develop/develop.h"
#include "develop/imageop.h"
#include "develop/masks.h"
#include "dt_mobile_parameter_policy.h"
#include "dt_mobile_crop_validation.h"
#include "dt_mobile_export.h"
#include "imageio/imageio_module.h"
#include "imageio/imageio_common.h"
#include "gui/draw.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

enum { MOBILE_HISTORY_LIMIT = 64 };
typedef struct mobile_history_state {
  GList *history;
  GList *iop_order;
  int history_end, iop_order_version;
  /* Original chroma arrays are value-owned; module pointers remain stable
   * throughout this adapter's single-instance develop lifetime. */
  dt_dev_chroma_t chroma;
} mobile_history_state;
struct dt_mobile_engine { dt_imgid_t image_id; dt_develop_t develop; atomic_bool cancelled; pthread_mutex_t operation_lock;
  dt_imageio_preview_cancel_t *preview_cancel;
  GPtrArray *history_states;
  guint history_position;
};
static pthread_mutex_t runtime_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t engine_work_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned runtime_users;
static gchar *runtime_directory;
/* Configuration outlives any one initialized darktable runtime. */
static gchar *runtime_data_directory;
static gchar *runtime_module_directory;
static gchar *runtime_temp_root;
static void sync_temperature_chroma(dt_develop_t *dev);

static void free_history_state(gpointer data)
{
  mobile_history_state *state = data;
  if(!state) return;
  g_list_free_full(state->history, dt_dev_free_history_item);
  g_list_free_full(state->iop_order, free);
  g_free(state);
}

static mobile_history_state *capture_history_state(dt_develop_t *dev)
{
  /* Use the same deep-copy helpers as the desktop history undo recorder.
   * Module pointers refer to this develop's instances, whose lifetime remains
   * stable: this adapter currently does not create/remove module instances. */
  mobile_history_state *state = g_new0(mobile_history_state, 1);
  state->history = dt_history_duplicate(dev->history);
  state->history_end = dev->history_end;
  state->iop_order = dt_ioppr_iop_order_copy_deep(dev->iop_order_list);
  state->iop_order_version = dev->iop_order_version;
  state->chroma = dev->chroma;
  return state;
}

static void record_history_state(dt_mobile_engine *engine)
{
  mobile_history_state *state = capture_history_state(&engine->develop);
  if(!engine->history_states)
    engine->history_states = g_ptr_array_new_with_free_func(free_history_state);
  else
    g_ptr_array_set_size(engine->history_states, engine->history_position + 1);
  g_ptr_array_add(engine->history_states, state);
  /* Retain the oldest reachable state plus 64 Apply steps. Dropping a state
   * also releases its params, blend settings and independently copied masks. */
  if(engine->history_states->len > MOBILE_HISTORY_LIMIT + 1)
    g_ptr_array_remove_index(engine->history_states, 0);
  engine->history_position = engine->history_states->len - 1;
}

static void remove_runtime_directory(const char *path)
{
  GDir *directory = g_dir_open(path, 0, NULL);
  if(directory)
  {
    const char *entry;
    while((entry = g_dir_read_name(directory)))
    {
      gchar *child = g_build_filename(path, entry, NULL);
      GStatBuf info;
      if(!g_lstat(child, &info) && S_ISDIR(info.st_mode)) remove_runtime_directory(child);
      else g_unlink(child);
      g_free(child);
    }
    g_dir_close(directory);
  }
  g_rmdir(path);
}

static void cleanup_runtime(void)
{
  dt_cleanup();
  if(runtime_directory)
  {
    remove_runtime_directory(runtime_directory);
    g_clear_pointer(&runtime_directory, g_free);
  }
}

static dt_mobile_status engine_fail(char *error, size_t size, dt_mobile_status status, const char *text)
{ if(error && size) snprintf(error, size, "%s", text); return status; }

static gboolean is_directory(const char *path)
{
  struct stat info;
  return path && path[0] && !stat(path, &info) && S_ISDIR(info.st_mode)
         && !g_access(path, R_OK | X_OK);
}

static gboolean has_runtime_data(const char *directory, char *error, size_t error_size)
{
  /* Necessary startup/RAW data, not a claim that four files provide full desktop
   * parity. The complete installed data tree must still be packaged. */
  static const char *required[] = {
    "darktablerc", "rawspeed/cameras.xml", "noiseprofiles.json", "wb_presets.json"
  };
  for(size_t k = 0; k < sizeof(required) / sizeof(required[0]); k++)
  {
    gchar *path = g_build_filename(directory, required[k], NULL);
    GStatBuf info;
    const gboolean valid = !g_stat(path, &info) && S_ISREG(info.st_mode)
                           && info.st_size > 0 && !g_access(path, R_OK);
    g_free(path);
    if(!valid)
    {
      if(error && error_size) snprintf(error, error_size, "runtime data is missing or unreadable: %s", required[k]);
      return FALSE;
    }
  }
#ifdef __ANDROID__
  /* Android cannot fall back to translations beside the system app_process
   * executable. Reject an incomplete private installation before dt_init,
   * whose desktop path resolver exits the process on missing directories. */
  char canonical_data[PATH_MAX];
  if(!realpath(directory, canonical_data))
  {
    if(error && error_size) snprintf(error, error_size, "unable to resolve runtime data directory");
    return FALSE;
  }
  gchar *runtime_root = g_path_get_dirname(canonical_data);
  gchar *localedir = g_build_filename(runtime_root, "locale", NULL);
  g_free(runtime_root);
  GDir *locales = g_dir_open(localedir, 0, NULL);
  gboolean has_catalog = FALSE;
  const char *locale;
  while(locales && !has_catalog && (locale = g_dir_read_name(locales)))
  {
    gchar *catalog = g_build_filename(localedir, locale, "LC_MESSAGES", "darktable.mo", NULL);
    GStatBuf info;
    has_catalog = !g_stat(catalog, &info) && S_ISREG(info.st_mode)
                  && info.st_size > 0 && !g_access(catalog, R_OK);
    g_free(catalog);
  }
  if(locales) g_dir_close(locales);
  g_free(localedir);
  if(!has_catalog)
  {
    if(error && error_size) snprintf(error, error_size, "runtime locale directory has no readable darktable catalog");
    return FALSE;
  }
#endif
  return TRUE;
}

dt_mobile_status dt_mobile_engine_configure_runtime(const char *data_dir, const char *module_dir,
                                                   const char *temp_root, char *error, size_t error_size)
{
  if(!data_dir || !module_dir || !temp_root || !g_path_is_absolute(data_dir)
     || !g_path_is_absolute(module_dir) || !g_path_is_absolute(temp_root))
    return engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                       "runtime directories must be absolute paths");
  /* Do not wait for a live init/cleanup and then unexpectedly reconfigure the
   * next session. The caller must deliberately retry once runtime work stops. */
  if(pthread_mutex_trylock(&runtime_lock))
    return engine_fail(error, error_size, DT_MOBILE_ERROR_ENGINE, "runtime initialization or cleanup is busy");
  dt_mobile_status status = DT_MOBILE_OK;
  if(runtime_users)
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_ENGINE,
                         "runtime configuration cannot change while an image session is active");
  else if(!is_directory(data_dir) || !is_directory(module_dir) || !is_directory(temp_root))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_IO, "runtime directories are missing or inaccessible");
  else if(!has_runtime_data(data_dir, error, error_size)) status = DT_MOBILE_ERROR_IO;
  else
  {
    gchar *probe = g_build_filename(temp_root, ".dt-mobile-write-XXXXXX", NULL);
    const int descriptor = g_mkstemp(probe);
    if(descriptor < 0)
      status = engine_fail(error, error_size, DT_MOBILE_ERROR_IO, "runtime temporary root is not writable");
    else
    {
      close(descriptor);
      if(g_unlink(probe))
        status = engine_fail(error, error_size, DT_MOBILE_ERROR_IO, "unable to remove runtime write probe");
    }
    g_free(probe);
    if(status == DT_MOBILE_OK)
    {
      gchar *data_copy = g_strdup(data_dir);
      gchar *module_copy = g_strdup(module_dir);
      gchar *temp_copy = g_strdup(temp_root);
      g_free(runtime_data_directory); runtime_data_directory = data_copy;
      g_free(runtime_module_directory); runtime_module_directory = module_copy;
      g_free(runtime_temp_root); runtime_temp_root = temp_copy;
      if(error && error_size) error[0] = '\0';
    }
  }
  pthread_mutex_unlock(&runtime_lock);
  return status;
}

dt_mobile_status dt_mobile_engine_open(const char *path, dt_mobile_engine **out,
                                        char *error, size_t error_size)
{
  *out = NULL;
  pthread_mutex_lock(&runtime_lock);
  if(!runtime_users)
  {
    const char *datadir = runtime_data_directory ? runtime_data_directory : g_getenv("DT_MOBILE_DATADIR");
    const char *moduledir = runtime_module_directory ? runtime_module_directory : g_getenv("DT_MOBILE_MODULEDIR");
    if(!is_directory(datadir) || !is_directory(moduledir))
    {
      pthread_mutex_unlock(&runtime_lock);
      return engine_fail(error, error_size, DT_MOBILE_ERROR_ENGINE,
                         "configure installed runtime data/modules before opening an image");
    }
    if(!has_runtime_data(datadir, error, error_size))
    { pthread_mutex_unlock(&runtime_lock); return DT_MOBILE_ERROR_IO; }
    /* A document editor must not open the desktop user's database/config or
     * write sidecars beside its cached source document. */
    gchar *runtime_template = g_build_filename(runtime_temp_root ? runtime_temp_root : g_get_tmp_dir(),
                                               "darktable-mobile-XXXXXX", NULL);
    runtime_directory = g_mkdtemp(runtime_template) ? runtime_template : NULL;
    if(!runtime_directory) g_free(runtime_template);
    gchar *configdir = runtime_directory ? g_build_filename(runtime_directory, "config", NULL) : NULL;
    gchar *cachedir = runtime_directory ? g_build_filename(runtime_directory, "cache", NULL) : NULL;
    if(!runtime_directory || g_mkdir_with_parents(configdir, 0700) || g_mkdir_with_parents(cachedir, 0700))
    {
      g_free(configdir); g_free(cachedir);
      if(runtime_directory) { remove_runtime_directory(runtime_directory); g_clear_pointer(&runtime_directory, g_free); }
      pthread_mutex_unlock(&runtime_lock);
      return engine_fail(error, error_size, DT_MOBILE_ERROR_IO, "unable to create private engine runtime directories");
    }
    char *argv[] = { (char *)"darktable-mobile", (char *)"--library", (char *)":memory:",
                     (char *)"--datadir", (char *)datadir,
                     (char *)"--moduledir", (char *)moduledir,
                     (char *)"--configdir", configdir, (char *)"--cachedir", cachedir,
                     (char *)"--tmpdir", runtime_directory,
                     (char *)"--conf", (char *)"write_sidecar_files=never", NULL };
    const int initialized = dt_init(15, argv, FALSE, TRUE, NULL);
    g_free(configdir); g_free(cachedir);
    if(initialized)
    {
      remove_runtime_directory(runtime_directory); g_clear_pointer(&runtime_directory, g_free);
      pthread_mutex_unlock(&runtime_lock);
      return engine_fail(error, error_size, DT_MOBILE_ERROR_ENGINE, "darktable headless initialization failed");
    }
  }
  runtime_users++;
  pthread_mutex_unlock(&runtime_lock);

  pthread_mutex_lock(&engine_work_lock);
  gchar *directory = g_path_get_dirname(path);
  dt_film_t film;
  const dt_filmid_t film_id = dt_film_new(&film, directory);
  g_free(directory);
  const dt_imgid_t image_id = dt_is_valid_filmid(film_id)
    ? dt_image_import(film_id, path, TRUE, FALSE) : NO_IMGID;
  pthread_mutex_unlock(&engine_work_lock);
  if(!dt_is_valid_imgid(image_id))
  {
    pthread_mutex_lock(&runtime_lock); runtime_users--; if(!runtime_users) cleanup_runtime(); pthread_mutex_unlock(&runtime_lock);
    return engine_fail(error, error_size, DT_MOBILE_ERROR_DECODE, "darktable could not import or decode the image");
  }
  dt_mobile_engine *engine = calloc(1, sizeof(*engine));
  if(!engine)
  {
    pthread_mutex_lock(&runtime_lock); runtime_users--; if(!runtime_users) cleanup_runtime(); pthread_mutex_unlock(&runtime_lock);
    return engine_fail(error, error_size, DT_MOBILE_ERROR_OUT_OF_MEMORY, "unable to allocate darktable image context");
  }
  engine->image_id = image_id;
  atomic_init(&engine->cancelled, false);
  pthread_mutex_init(&engine->operation_lock, NULL);
  engine->preview_cancel = dt_imageio_preview_cancel_new();
  pthread_mutex_lock(&engine_work_lock);
  dt_dev_init(&engine->develop, FALSE);
  dt_dev_load_image(&engine->develop, image_id);
  /* Loading history constructs its items. Apply the current history end to
   * module params before exposing controls or editing a single field, so all
   * other values from imported history and auto presets are preserved. */
  dt_dev_pop_history_items_ext(&engine->develop, engine->develop.history_end);
  const gboolean decoded = engine->develop.image_storage.load_status == DT_IMAGEIO_OK;
  if(decoded && engine->preview_cancel)
  {
    sync_temperature_chroma(&engine->develop);
    record_history_state(engine);
  }
  pthread_mutex_unlock(&engine_work_lock);
  if(!decoded || !engine->preview_cancel)
  {
    dt_mobile_engine_close(engine);
    return engine_fail(error, error_size, decoded ? DT_MOBILE_ERROR_OUT_OF_MEMORY : DT_MOBILE_ERROR_DECODE,
                       decoded ? "unable to allocate cancellation state" : "darktable could not decode full image data");
  }
  *out = engine;
  return DT_MOBILE_OK;
}

/* Module parameters stay in the upstream parameter structs. The JSON is only
 * a description for touch controls, never a second implementation of an iop. */
static void json_string(GString *json, const char *text)
{
  g_string_append_c(json, '"');
  for(const unsigned char *p = (const unsigned char *)(text ? text : ""); *p; p++)
  {
    if(*p == '"' || *p == '\\') g_string_append_c(json, '\\');
    if(*p < 0x20) g_string_append_printf(json, "\\u%04x", *p);
    else g_string_append_c(json, *p);
  }
  g_string_append_c(json, '"');
}

static void json_number(GString *json, double value)
{
  char number[G_ASCII_DTOSTR_BUF_SIZE];
  g_string_append(json, isfinite(value) ? g_ascii_dtostr(number, sizeof(number), value) : "null");
}

static dt_mobile_status finish_json(GString *json, char **out, char *error, size_t size)
{
  *out = malloc(json->len + 1);
  if(*out) memcpy(*out, json->str, json->len + 1);
  g_string_free(json, TRUE);
  return *out ? DT_MOBILE_OK : engine_fail(error, size, DT_MOBILE_ERROR_OUT_OF_MEMORY,
                                           "unable to allocate module description");
}

static void module_identifier(const dt_iop_module_t *module, char *identifier, size_t size)
{
  if(module->multi_priority)
    snprintf(identifier, size, "%s@%d", module->op, module->multi_priority);
  else snprintf(identifier, size, "%s", module->op);
}

static dt_iop_module_t *find_module(dt_mobile_engine *engine, const char *identifier)
{
  for(GList *item = engine->develop.iop; item; item = g_list_next(item))
  {
    dt_iop_module_t *module = item->data;
    char current[128];
    module_identifier(module, current, sizeof(current));
    if(!strcmp(current, identifier)) return module;
  }
  return NULL;
}

static size_t enum_entry_count(const dt_introspection_field_t *field)
{
  if(!field->Enum.values) return 0;
  size_t count = 0;
  /* The upstream introspection API terminates values with a NULL name. Honor
   * that terminator as well as entries, including when an older generator
   * supplied an inconsistent count for an anonymous enum. */
  while(count < field->Enum.entries && field->Enum.values[count].name) count++;
  return count;
}

static gboolean number_info(const dt_introspection_field_t *field, const void *value,
                            long double *current, long double *minimum, long double *maximum,
                            const char **type)
{
  *type = "int";
#define NUMBER_INFO(tag, ctype, member) \
  case DT_INTROSPECTION_TYPE_##tag: \
    *current = *(const ctype *)value; \
    *minimum = field->member.Min; *maximum = field->member.Max; return TRUE
  switch(field->header.type)
  {
    case DT_INTROSPECTION_TYPE_FLOAT:
      *type = "float"; *current = *(const float *)value;
      *minimum = field->Float.Min; *maximum = field->Float.Max; return TRUE;
    case DT_INTROSPECTION_TYPE_DOUBLE:
      *type = "double"; *current = *(const double *)value;
      *minimum = field->Double.Min; *maximum = field->Double.Max; return TRUE;
    NUMBER_INFO(CHAR, char, Char);
    NUMBER_INFO(INT8, int8_t, Int8);
    NUMBER_INFO(UINT8, uint8_t, UInt8);
    NUMBER_INFO(SHORT, short, Short);
    NUMBER_INFO(USHORT, unsigned short, UShort);
    NUMBER_INFO(INT, int, Int);
    NUMBER_INFO(UINT, unsigned int, UInt);
    NUMBER_INFO(LONG, long, Long);
    NUMBER_INFO(ULONG, unsigned long, ULong);
    case DT_INTROSPECTION_TYPE_BOOL:
      *type = "bool"; *current = *(const gboolean *)value;
      *minimum = 0; *maximum = 1; return TRUE;
    case DT_INTROSPECTION_TYPE_ENUM:
    {
      const size_t entries = enum_entry_count(field);
      if(!entries) return FALSE;
      *type = "enum"; *current = *(const int *)value;
      *minimum = *maximum = field->Enum.values[0].value;
      for(size_t k = 1; k < entries; k++)
      {
        *minimum = MIN(*minimum, field->Enum.values[k].value);
        *maximum = MAX(*maximum, field->Enum.values[k].value);
      }
      return TRUE;
    }
    default: return FALSE;
  }
#undef NUMBER_INFO
}

typedef void (*field_visitor)(dt_iop_module_t *, const dt_introspection_field_t *,
                              size_t, const char *, void *);

/* Introspection offsets are absolute within the parameter struct, including
 * nested structs. Array element descriptors describe the first element. */
static void visit_fields(dt_iop_module_t *module, const dt_introspection_field_t *field,
                         size_t delta, const char *path, unsigned depth,
                         field_visitor visitor, void *data)
{
  if(!field || depth > 32 || delta > (size_t)module->params_size
     || field->header.offset > (size_t)module->params_size - delta) return;
  if(field->header.type == DT_INTROSPECTION_TYPE_STRUCT)
  {
    for(size_t k = 0; k < field->Struct.entries; k++)
    {
      const dt_introspection_field_t *child = field->Struct.fields[k];
      gchar *name = path[0] ? g_strconcat(path, ".", child->header.field_name, NULL)
                             : g_strdup(child->header.field_name);
      visit_fields(module, child, delta, name, depth + 1, visitor, data);
      g_free(name);
    }
  }
  else if(field->header.type == DT_INTROSPECTION_TYPE_ARRAY)
  {
    /* Strings and opaque LUTs need purpose-built controls. Do not present a
     * partial list as a complete editor for those parameter types. */
    if(!field->Array.count || field->Array.count > 4096
       || field->Array.type == DT_INTROSPECTION_TYPE_CHAR) return;
    const size_t stride = field->header.size / field->Array.count;
    for(size_t k = 0; k < field->Array.count; k++)
    {
      if(k && stride > (SIZE_MAX - delta) / k) break;
      gchar *name = g_strdup_printf("%s[%zu]", path, k);
      visit_fields(module, field->Array.field, delta + k * stride, name,
                   depth + 1, visitor, data);
      g_free(name);
    }
  }
  else if(field->header.size <= (size_t)module->params_size - delta - field->header.offset)
    visitor(module, field, field->header.offset + delta, path, data);
}

typedef struct parameter_json { GString *json; gboolean first; } parameter_json;
static void append_parameter(dt_iop_module_t *module, const dt_introspection_field_t *field,
                             size_t offset, const char *path, void *data)
{
  if(!dt_mobile_parameter_allowed(module->op, path, field->header.type)) return;
  parameter_json *description = data;
  long double value, minimum, maximum;
  const char *type;
  if(!number_info(field, (const char *)module->params + offset, &value, &minimum, &maximum, &type)
     || !isfinite((double)value)) return;
  /* Some iops use NaN as an unavailable sensor channel (for example the
   * fourth white-balance coefficient on a three-channel RGB image). Such a
   * sentinel is not an editable numeric control. */
  GString *json = description->json;
  if(!description->first) g_string_append_c(json, ',');
  description->first = FALSE;
  g_string_append(json, "{\"name\":"); json_string(json, path);
  g_string_append(json, ",\"label\":");
  json_string(json, field->header.description && field->header.description[0]
                     ? field->header.description : path);
  g_string_append(json, ",\"type\":"); json_string(json, type);
  g_string_append(json, ",\"value\":"); json_number(json, (double)value);
  g_string_append(json, ",\"min\":"); json_number(json, (double)minimum);
  g_string_append(json, ",\"max\":"); json_number(json, (double)maximum);
  if(field->header.type == DT_INTROSPECTION_TYPE_ENUM)
  {
    g_string_append(json, ",\"choices\":[");
    for(size_t k = 0; k < enum_entry_count(field); k++)
    {
      const dt_introspection_type_enum_tuple_t *choice = &field->Enum.values[k];
      if(k) g_string_append_c(json, ',');
      g_string_append_printf(json, "{\"value\":%d,\"label\":", choice->value);
      json_string(json, choice->description && choice->description[0] ? choice->description : choice->name);
      g_string_append_c(json, '}');
    }
    g_string_append_c(json, ']');
  }
  g_string_append_c(json, '}');
}

enum { MOBILE_CURVE_CHANNELS = 3, MOBILE_CURVE_MAX_NODES = 20, MOBILE_CURVE_SAMPLES = 256 };
static const float mobile_curve_min_spacing = 1e-6f;
typedef struct mobile_curve_layout
{
  size_t nodes, channel_stride, node_stride, x, y, counts, types;
  int channels, editable_channels;
  gboolean rgb;
} mobile_curve_layout;

static gboolean curve_field_fits(const dt_iop_module_t *module, const dt_introspection_field_t *field)
{
  return field && field->header.offset <= (size_t)module->params_size
    && field->header.size <= (size_t)module->params_size - field->header.offset;
}

/* These two upstream tools share the shape of a 3 x 20 node array plus
 * per-channel counts/interpolation. Resolve that shape through introspection,
 * rather than duplicating version-sensitive iop parameter structs here. */
static gboolean curve_layout(dt_iop_module_t *module, mobile_curve_layout *layout)
{
  if(!module || !module->have_introspection || !module->get_introspection()
     || !module->get_f || !module->params) return FALSE;
  const gboolean rgb = !strcmp(module->op, "rgbcurve");
  if(!rgb && strcmp(module->op, "basecurve")) return FALSE;
  const char *node_name = rgb ? "curve_nodes" : "basecurve";
  const dt_introspection_field_t *nodes = module->get_f(node_name);
  const dt_introspection_field_t *counts = module->get_f(rgb ? "curve_num_nodes" : "basecurve_nodes");
  const dt_introspection_field_t *types = module->get_f(rgb ? "curve_type" : "basecurve_type");
  if(!curve_field_fits(module, nodes) || !curve_field_fits(module, counts)
     || !curve_field_fits(module, types)
     || nodes->header.type != DT_INTROSPECTION_TYPE_ARRAY || nodes->Array.count != MOBILE_CURVE_CHANNELS
     || nodes->Array.type != DT_INTROSPECTION_TYPE_ARRAY || !nodes->Array.field) return FALSE;
  const dt_introspection_field_t *row = nodes->Array.field;
  if(!curve_field_fits(module, row) || row->header.type != DT_INTROSPECTION_TYPE_ARRAY || row->Array.count != MOBILE_CURVE_MAX_NODES
     || row->Array.type != DT_INTROSPECTION_TYPE_STRUCT || !row->Array.field
     || row->header.offset != nodes->header.offset
     || nodes->header.size != MOBILE_CURVE_CHANNELS * row->header.size) return FALSE;
  const dt_introspection_field_t *node = row->Array.field;
  if(!curve_field_fits(module, node) || node->header.type != DT_INTROSPECTION_TYPE_STRUCT || node->Struct.entries != 2
     || !node->Struct.fields || node->header.offset != row->header.offset
     || row->header.size != MOBILE_CURVE_MAX_NODES * node->header.size) return FALSE;
  const dt_introspection_field_t *x = NULL, *y = NULL;
  for(size_t k = 0; k < node->Struct.entries; k++)
  {
    const dt_introspection_field_t *field = node->Struct.fields[k];
    if(!field || !field->header.field_name || field->header.type != DT_INTROSPECTION_TYPE_FLOAT
       || field->header.size != sizeof(float) || field->header.offset < node->header.offset
       || field->header.offset - node->header.offset > node->header.size
       || sizeof(float) > node->header.size - (field->header.offset - node->header.offset)) return FALSE;
    if(!strcmp(field->header.field_name, "x")) x = field;
    else if(!strcmp(field->header.field_name, "y")) y = field;
  }
  if(!x || !y || x->header.offset == y->header.offset) return FALSE;
  const dt_introspection_field_t *arrays[] = { counts, types };
  for(size_t k = 0; k < 2; k++)
  {
    const dt_introspection_field_t *array = arrays[k];
    if(array->header.type != DT_INTROSPECTION_TYPE_ARRAY || array->Array.count != MOBILE_CURVE_CHANNELS
       || array->Array.type != DT_INTROSPECTION_TYPE_INT || !array->Array.field
       || array->Array.field->header.type != DT_INTROSPECTION_TYPE_INT
       || array->Array.field->header.size != sizeof(int)
       || array->Array.field->header.offset != array->header.offset
       || array->header.size != MOBILE_CURVE_CHANNELS * sizeof(int)) return FALSE;
  }
  int editable_channels = 1;
  if(rgb)
  {
    const dt_introspection_field_t *mode = module->get_f("curve_autoscale");
    int value;
    if(!curve_field_fits(module, mode) || mode->header.type != DT_INTROSPECTION_TYPE_ENUM
       || mode->header.size != sizeof(value)) return FALSE;
    memcpy(&value, (const char *)module->params + mode->header.offset, sizeof(value));
    if(value != 0 && value != 1) return FALSE; /* Upstream linked/independent modes. */
    editable_channels = value == 0 ? 1 : MOBILE_CURVE_CHANNELS;
  }
  *layout = (mobile_curve_layout) { nodes->header.offset, row->header.size, node->header.size,
    x->header.offset - node->header.offset, y->header.offset - node->header.offset,
    counts->header.offset, types->header.offset, rgb ? MOBILE_CURVE_CHANNELS : 1, editable_channels, rgb };
  return TRUE;
}

static gboolean valid_curve_points(const dt_mobile_curve_point *points, size_t count, int interpolation)
{
  if(!points || count < 2 || count > MOBILE_CURVE_MAX_NODES || interpolation < 0 || interpolation > 2)
    return FALSE;
  for(size_t k = 0; k < count; k++)
    if(!isfinite(points[k].x) || !isfinite(points[k].y) || points[k].x < 0.0f || points[k].x > 1.0f
       || points[k].y < 0.0f || points[k].y > 1.0f
       || (k && (double)points[k].x - (double)points[k - 1].x < (double)mobile_curve_min_spacing))
      return FALSE;
  return TRUE;
}

static gboolean read_curve(const dt_iop_module_t *module, const mobile_curve_layout *layout, int channel,
                           dt_mobile_curve_point *points, int *count, int *interpolation)
{
  const char *params = module->params;
  memcpy(count, params + layout->counts + channel * sizeof(int), sizeof(*count));
  memcpy(interpolation, params + layout->types + channel * sizeof(int), sizeof(*interpolation));
  if(*count < 2 || *count > MOBILE_CURVE_MAX_NODES) return FALSE;
  for(int k = 0; k < *count; k++)
  {
    const char *node = params + layout->nodes + channel * layout->channel_stride + k * layout->node_stride;
    memcpy(&points[k].x, node + layout->x, sizeof(float));
    memcpy(&points[k].y, node + layout->y, sizeof(float));
  }
  return valid_curve_points(points, (size_t)*count, *interpolation);
}

static gboolean curve_samples(const dt_mobile_curve_point *points, size_t count, int interpolation,
                              float *values, float *endpoint)
{
  dt_draw_curve_t *curve = dt_draw_curve_new(0.0f, 1.0f, (unsigned int)interpolation);
  for(size_t k = 0; k < count; k++) dt_draw_curve_add_point(curve, points[k].x, points[k].y);
  dt_draw_curve_calc_values(curve, 0.0f, 1.0f, MOBILE_CURVE_SAMPLES, NULL, values);
  *endpoint = dt_draw_curve_calc_value(curve, 1.0f);
  dt_draw_curve_destroy(curve);
  if(!isfinite(*endpoint)) return FALSE;
  for(size_t k = 0; k < MOBILE_CURVE_SAMPLES; k++) if(!isfinite(values[k])) return FALSE;
  return TRUE;
}

typedef struct mobile_rgb_curve_options_layout
{
  const dt_introspection_field_t *mode, *preserve;
  int current_mode, current_preserve;
} mobile_rgb_curve_options_layout;

static gboolean enum_has_value(const dt_introspection_field_t *field, int value)
{
  for(size_t k = 0; k < enum_entry_count(field); k++)
    if(field->Enum.values[k].value == value) return TRUE;
  return FALSE;
}

static gboolean rgb_curve_options_layout(dt_iop_module_t *module, mobile_curve_layout *curves,
                                         mobile_rgb_curve_options_layout *options)
{
  if(!curve_layout(module, curves) || !curves->rgb) return FALSE;
  const char *names[] = { "curve_autoscale", "preserve_colors" };
  const dt_introspection_field_t *fields[2];
  for(size_t k = 0; k < 2; k++)
  {
    fields[k] = module->get_f(names[k]);
    if(!curve_field_fits(module, fields[k]) || !fields[k]->header.field_name
       || strcmp(fields[k]->header.field_name, names[k])
       || fields[k]->header.type != DT_INTROSPECTION_TYPE_ENUM
       || fields[k]->header.size != sizeof(int)) return FALSE;
  }
  if(fields[0]->header.offset == fields[1]->header.offset
     || enum_entry_count(fields[0]) != 2 || !enum_has_value(fields[0], 0) || !enum_has_value(fields[0], 1)
     || enum_entry_count(fields[1]) != 7) return FALSE;
  /* Match the reviewed common/rgb_norms.h choices, not a min/max range that
   * could admit holes or newly added processing modes without review. */
  const int preserve_values[] = { 0, 1, 2, 3, 4, 5, 6 };
  for(size_t k = 0; k < sizeof(preserve_values) / sizeof(preserve_values[0]); k++)
    if(!enum_has_value(fields[1], preserve_values[k])) return FALSE;
  int mode, preserve;
  memcpy(&mode, (const char *)module->params + fields[0]->header.offset, sizeof(mode));
  memcpy(&preserve, (const char *)module->params + fields[1]->header.offset, sizeof(preserve));
  if(!enum_has_value(fields[0], mode) || !enum_has_value(fields[1], preserve)) return FALSE;
  /* Processing builds all three LUTs, including inactive linked-mode channels.
   * A mode change must not enable malformed untouched historical channels. */
  for(int channel = 0; channel < MOBILE_CURVE_CHANNELS; channel++)
  {
    dt_mobile_curve_point points[MOBILE_CURVE_MAX_NODES];
    int count, interpolation;
    if(!read_curve(module, curves, channel, points, &count, &interpolation)) return FALSE;
  }
  *options = (mobile_rgb_curve_options_layout){ fields[0], fields[1], mode, preserve };
  return TRUE;
}

static void append_enum_choices(GString *json, const dt_introspection_field_t *field)
{
  g_string_append_c(json, '[');
  for(size_t k = 0; k < enum_entry_count(field); k++)
  {
    const dt_introspection_type_enum_tuple_t *choice = &field->Enum.values[k];
    if(k) g_string_append_c(json, ',');
    g_string_append_printf(json, "{\"value\":%d,\"label\":", choice->value);
    json_string(json, choice->description && choice->description[0] ? choice->description : choice->name);
    g_string_append_c(json, '}');
  }
  g_string_append_c(json, ']');
}

static void append_rgb_curve_options(dt_iop_module_t *module, GString *json)
{
  mobile_curve_layout curves;
  mobile_rgb_curve_options_layout options;
  if(!rgb_curve_options_layout(module, &curves, &options)) return;
  g_string_append_printf(json, ",\"rgbCurveOptions\":{\"mode\":%d,\"preserveColors\":%d,\"modes\":",
                         options.current_mode, options.current_preserve);
  append_enum_choices(json, options.mode);
  g_string_append(json, ",\"preserveColorChoices\":");
  append_enum_choices(json, options.preserve);
  g_string_append_c(json, '}');
}

static void append_curve_point(GString *json, float x, float y)
{
  g_string_append(json, "{\"x\":"); json_number(json, x);
  g_string_append(json, ",\"y\":"); json_number(json, y);
  g_string_append_c(json, '}');
}

static void append_curves(dt_iop_module_t *module, GString *json)
{
  mobile_curve_layout layout;
  if(!curve_layout(module, &layout)) return;
  gboolean first = TRUE;
  /* Linked RGB mode applies channel zero to every component; its stored
   * independent channels are inactive and are not editing targets. */
  for(int channel = 0; channel < layout.editable_channels; channel++)
  {
    dt_mobile_curve_point points[MOBILE_CURVE_MAX_NODES];
    int count, interpolation;
    float samples[MOBILE_CURVE_SAMPLES], endpoint;
    /* Unsupported historical shapes are omitted without changing that history
     * or making an unrelated module's metadata unavailable. */
    if(!read_curve(module, &layout, channel, points, &count, &interpolation)
       || !curve_samples(points, (size_t)count, interpolation, samples, &endpoint)) continue;
    if(!first) g_string_append_c(json, ',');
    first = FALSE;
    g_string_append_printf(json, "{\"channel\":%d,\"label\":", channel);
    const char *label = layout.rgb && layout.editable_channels == MOBILE_CURVE_CHANNELS
      ? (channel == 0 ? "Red" : channel == 1 ? "Green" : "Blue") : "RGB";
    json_string(json, label);
    g_string_append_printf(json, ",\"editable\":%s,\"interpolation\":%d,\"maxNodes\":%d,\"minSpacing\":",
      "true", interpolation, MOBILE_CURVE_MAX_NODES);
    json_number(json, mobile_curve_min_spacing);
    g_string_append(json, ",\"nodes\":[");
    for(int k = 0; k < count; k++)
    {
      if(k) g_string_append_c(json, ',');
      append_curve_point(json, points[k].x, points[k].y);
    }
    g_string_append(json, "],\"samples\":[");
    for(int k = 0; k < MOBILE_CURVE_SAMPLES; k++)
    {
      if(k) g_string_append_c(json, ',');
      append_curve_point(json, (float)k / MOBILE_CURVE_SAMPLES, samples[k]);
    }
    g_string_append_c(json, ','); append_curve_point(json, 1.0f, endpoint);
    g_string_append(json, "]}");
  }
}

typedef struct mobile_crop_layout { size_t coordinates[4], ratios[2]; } mobile_crop_layout;

/* Resolve the six direct fields of the upstream crop module, not a copied
 * parameter struct. A future schema/type change disables this editor instead
 * of writing a partially compatible history blob. */
static gboolean crop_layout(dt_iop_module_t *module, mobile_crop_layout *layout)
{
  if(!module || strcmp(module->op, "crop") || !module->have_introspection
     || !module->get_introspection || !module->get_introspection()
     || !module->get_f || !module->params || module->params_size <= 0) return FALSE;
  const char *names[] = { "cx", "cy", "cw", "ch", "ratio_n", "ratio_d" };
  const dt_introspection_field_t *fields[6];
  for(size_t k = 0; k < 6; k++)
  {
    fields[k] = module->get_f(names[k]);
    if(!curve_field_fits(module, fields[k]) || !fields[k]->header.field_name
       || strcmp(fields[k]->header.field_name, names[k])) return FALSE;
    if(k < 4)
    {
      if(fields[k]->header.type != DT_INTROSPECTION_TYPE_FLOAT
         || fields[k]->header.size != sizeof(float)
         || fields[k]->Float.Min != 0.0f || fields[k]->Float.Max != 1.0f) return FALSE;
      layout->coordinates[k] = fields[k]->header.offset;
    }
    else
    {
      if(fields[k]->header.type != DT_INTROSPECTION_TYPE_INT
         || fields[k]->header.size != sizeof(int)
         || fields[k]->Int.Min > 0 || fields[k]->Int.Max < 0) return FALSE;
      layout->ratios[k - 4] = fields[k]->header.offset;
    }
    for(size_t other = 0; other < k; other++)
      if(fields[k]->header.offset < fields[other]->header.offset + fields[other]->header.size
         && fields[other]->header.offset < fields[k]->header.offset + fields[k]->header.size) return FALSE;
  }
  return TRUE;
}

static void append_crop(dt_iop_module_t *module, GString *json)
{
  mobile_crop_layout layout;
  if(!crop_layout(module, &layout)) return;
  float coordinates[4];
  int ratios[2];
  for(size_t k = 0; k < 4; k++)
    memcpy(&coordinates[k], (const char *)module->params + layout.coordinates[k], sizeof(float));
  if(!dt_mobile_crop_valid(coordinates[0], coordinates[1], coordinates[2], coordinates[3])) return;
  for(size_t k = 0; k < 2; k++)
    memcpy(&ratios[k], (const char *)module->params + layout.ratios[k], sizeof(int));
  g_string_append(json, ",\"crop\":{\"left\":"); json_number(json, coordinates[0]);
  g_string_append(json, ",\"top\":"); json_number(json, coordinates[1]);
  g_string_append(json, ",\"right\":"); json_number(json, coordinates[2]);
  g_string_append(json, ",\"bottom\":"); json_number(json, coordinates[3]);
  g_string_append(json, ",\"minSize\":"); json_number(json, DT_MOBILE_CROP_MIN_SIZE);
  /* commit_params also treats 0/n as freehand unless n selects either
   * orientation of the original image ratio (1 or -1). */
  const gboolean freehand = ratios[0] == 0 && ratios[1] != 1 && ratios[1] != -1;
  g_string_append_printf(json, ",\"freehand\":%s}", freehand ? "true" : "false");
}

typedef struct mobile_whitebalance_layout
{
  size_t gains[4], preset;
  int channels;
} mobile_whitebalance_layout;

static gboolean whitebalance_layout(dt_iop_module_t *module, mobile_whitebalance_layout *layout)
{
  if(!module || strcmp(module->op, "temperature") || !module->dev || !module->params
     || module->params_size <= 0 || !module->have_introspection
     || !module->get_introspection || !module->get_introspection() || !module->get_f) return FALSE;
  const char *names[] = { "red", "green", "blue", "various", "preset" };
  const dt_introspection_field_t *fields[5];
  for(size_t k = 0; k < 5; k++)
  {
    fields[k] = module->get_f(names[k]);
    if(!curve_field_fits(module, fields[k]) || !fields[k]->header.field_name
       || strcmp(fields[k]->header.field_name, names[k])) return FALSE;
    if(k < 4)
    {
      if(fields[k]->header.type != DT_INTROSPECTION_TYPE_FLOAT
         || fields[k]->header.size != sizeof(float)
         || fields[k]->Float.Min != 0.0f || fields[k]->Float.Max != 8.0f) return FALSE;
      layout->gains[k] = fields[k]->header.offset;
    }
    else
    {
      if(fields[k]->header.type != DT_INTROSPECTION_TYPE_INT
         || fields[k]->header.size != sizeof(int)) return FALSE;
      layout->preset = fields[k]->header.offset;
    }
    for(size_t previous = 0; previous < k; previous++)
      if(fields[k]->header.offset < fields[previous]->header.offset + fields[previous]->header.size
         && fields[previous]->header.offset < fields[k]->header.offset + fields[k]->header.size) return FALSE;
  }
  layout->channels = module->dev->image_storage.flags & DT_IMAGE_4BAYER ? 4 : 3;
  return TRUE;
}

static gboolean whitebalance_available(const dt_iop_module_t *module)
{
  return module && module->dev
    && !(dt_image_monochrome_flags(&module->dev->image_storage) & DT_IMAGE_MONOCHROME);
}

static const char *whitebalance_setting_label(int setting)
{
  /* Stable built-in setting IDs match temperature.c. USER and SPOT describe
   * existing state but are not client-selectable named coefficient sources. */
  switch(setting)
  {
    case 0: return "as shot";
    case 1: return "by spot";
    case 2: return "user defined";
    case 3: return "camera reference";
    case 4: return "as shot to reference";
    default: return "other";
  }
}

static gboolean whitebalance_basis(const dt_iop_module_t *module,
                                  const mobile_whitebalance_layout *layout,
                                  int setting, float gains[4])
{
  if(setting != 0 && setting != 3 && setting != 4) return FALSE;
  const dt_dev_chroma_t *chroma = &module->dev->chroma;
  const double *basis = setting == 3 ? chroma->D65coeffs : chroma->as_shot;
  for(int channel = 0; channel < layout->channels; channel++)
  {
    /* Original named coefficients are not the manual slider range. Preserve
     * valid values above 8, and reject overflow/underflow before any write. */
    if(!isfinite(basis[channel]) || basis[channel] <= 0.0 || basis[channel] > FLT_MAX) return FALSE;
    gains[channel] = (float)basis[channel];
    if(!isnormal(gains[channel]) || gains[channel] <= 0.0f) return FALSE;
  }
  return TRUE;
}

/* Use the same camera matrices and sRGB fallback as temperature.c. A headless
 * module has no gui_data, so the shared numerical core accepts matrices rather
 * than reaching through GUI state. Missing/singular calibration cannot be
 * advertised as a valid Kelvin editor. */
static gboolean whitebalance_matrices(dt_iop_module_t *module,
                                      double XYZ_to_CAM[4][3], double CAM_to_XYZ[3][4])
{
  dt_image_t *image = &module->dev->image_storage;
  if(!dt_image_is_raw(image))
    dt_whitebalance_srgb_matrices(XYZ_to_CAM, CAM_to_XYZ);
  else if(!dt_colorspaces_conversion_matrices_xyz(image->adobe_XYZ_to_CAM,
                                                  image->d65_color_matrix,
                                                  XYZ_to_CAM, CAM_to_XYZ)) return FALSE;
  const int channels = image->flags & DT_IMAGE_4BAYER ? 4 : 3;
  for(int row = 0; row < 4; row++)
  {
    gboolean nonzero = FALSE;
    for(int col = 0; col < 3; col++)
    {
      if(!isfinite(XYZ_to_CAM[row][col]) || !isfinite(CAM_to_XYZ[col][row])) return FALSE;
      if(XYZ_to_CAM[row][col] != 0.0) nonzero = TRUE;
    }
    /* A 3x3 embedded calibration has a zero inactive row. It cannot produce
     * valid four-channel gains for a real four-Bayer image. */
    if(row < channels && !nonzero) return FALSE;
  }
  return TRUE;
}

static void append_whitebalance_temperature(dt_iop_module_t *module,
                                            const mobile_whitebalance_layout *layout,
                                            const float gains[4], GString *json)
{
  double XYZ_to_CAM[4][3], CAM_to_XYZ[3][4];
  if(!whitebalance_matrices(module, XYZ_to_CAM, CAM_to_XYZ)) return;
  double multipliers[4];
  for(int channel = 0; channel < 4; channel++)
  {
    if(channel < layout->channels && (!isnormal(gains[channel]) || gains[channel] <= 0.0f)) return;
    /* The desktop inversion reads all four parameter fields. Its shared core
     * maps an inactive NaN/nonpositive sentinel to zero camera light. */
    multipliers[channel] = gains[channel];
  }
  const cmsCIEXYZ xyz = dt_whitebalance_multipliers_to_XYZ(CAM_to_XYZ, multipliers);
  if(!isfinite(xyz.X) || !isfinite(xyz.Y) || !isfinite(xyz.Z)
     || xyz.X <= 0.0 || xyz.Y <= 0.0 || xyz.Z <= 0.0) return;
  float kelvin, tint;
  dt_whitebalance_XYZ_to_temperature(xyz, &kelvin, &tint);
  if(!isfinite(kelvin) || !isfinite(tint)) return;
  g_string_append(json, ",\"temperature\":{\"kelvin\":"); json_number(json, kelvin);
  g_string_append(json, ",\"tint\":"); json_number(json, tint);
  g_string_append_printf(json, ",\"minKelvin\":%d,\"maxKelvin\":%d",
                         DT_IOP_LOWEST_TEMPERATURE, DT_IOP_HIGHEST_TEMPERATURE);
  g_string_append(json, ",\"minTint\":"); json_number(json, DT_IOP_LOWEST_TINT);
  g_string_append(json, ",\"maxTint\":"); json_number(json, DT_IOP_HIGHEST_TINT);
  g_string_append_c(json, '}');
}

static void append_whitebalance(dt_iop_module_t *module, GString *json)
{
  mobile_whitebalance_layout layout;
  if(!whitebalance_layout(module, &layout) || !whitebalance_available(module)) return;
  float gains[4];
  for(int channel = 0; channel < 4; channel++)
  {
    memcpy(&gains[channel], (const char *)module->params + layout.gains[channel], sizeof(float));
    if(channel < layout.channels && !isfinite(gains[channel])) return; /* Manual batch can repair this state. */
  }
  int setting;
  memcpy(&setting, (const char *)module->params + layout.preset, sizeof(setting));
  g_string_append(json, ",\"whiteBalance\":{\"channels\":[");
  const gboolean cygm = layout.channels == 4 && FILTERS_ARE_CYGM(module->dev->image_storage.buf_dsc.filters);
  const int cygm_order[] = { 2, 3, 1, 0 };
  const char *rgb_labels[] = { "red", "green", "blue", "emerald" };
  const char *cygm_labels[] = { "green", "magenta", "cyan", "yellow" };
  for(int display = 0; display < layout.channels; display++)
  {
    const int channel = cygm ? cygm_order[display] : display;
    if(display) g_string_append_c(json, ',');
    g_string_append_printf(json, "{\"index\":%d,\"label\":", channel);
    json_string(json, cygm ? cygm_labels[channel] : rgb_labels[channel]);
    g_string_append(json, ",\"value\":"); json_number(json, gains[channel]);
    g_string_append_c(json, '}');
  }
  g_string_append(json, "],\"manualMin\":"); json_number(json, 0.001f);
  g_string_append(json, ",\"manualMax\":"); json_number(json, 8.0f);
  g_string_append_printf(json, ",\"setting\":%d,\"settingLabel\":", setting);
  json_string(json, whitebalance_setting_label(setting));
  g_string_append(json, ",\"settings\":[");
  const int settings[] = { 0, 3, 4 };
  gboolean first = TRUE;
  for(size_t k = 0; k < sizeof(settings) / sizeof(settings[0]); k++)
  {
    float basis[4];
    if(!whitebalance_basis(module, &layout, settings[k], basis)) continue;
    if(!first) g_string_append_c(json, ',');
    first = FALSE;
    g_string_append_printf(json, "{\"id\":%d,\"label\":", settings[k]);
    json_string(json, whitebalance_setting_label(settings[k]));
    g_string_append_c(json, '}');
  }
  g_string_append_c(json, ']');
  append_whitebalance_temperature(module, &layout, gains, json);
  g_string_append_c(json, '}');
}

static void sync_temperature_chroma(dt_develop_t *dev)
{
  /* A headless develop has no persistent pixelpipe commit. Derive the same
   * temperature-owned state as temperature.c commit_params after restoring or
   * editing params. Keep original as-shot/D65 bases and adaptation ownership. */
  for(GList *item = dev->iop; item; item = item->next)
  {
    dt_iop_module_t *module = item->data;
    if(strcmp(module->op, "temperature")) continue;
    if(!whitebalance_available(module) || module->hide_enable_button)
    {
      for(int channel = 0; channel < 4; channel++) dev->chroma.wb_coeffs[channel] = 1.0f;
      return; /* The original true-monochrome commit also returns here. */
    }
    mobile_whitebalance_layout layout;
    if(!whitebalance_layout(module, &layout)) return;
    for(int channel = 0; channel < 4; channel++)
    {
      float gain;
      memcpy(&gain, (const char *)module->params + layout.gains[channel], sizeof(gain));
      dev->chroma.wb_coeffs[channel] = module->enabled ? gain : 1.0f;
    }
    int setting;
    memcpy(&setting, (const char *)module->params + layout.preset, sizeof(setting));
    dev->chroma.late_correction = setting == 4;
    dev->chroma.temperature = module->enabled ? module : NULL;
    return;
  }
}

/* All entry points take locks in the same order. Cancellation deliberately
 * takes neither lock so it can interrupt an active upstream pixelpipe. */
static dt_mobile_status begin_operation(dt_mobile_engine *engine, char *error, size_t size)
{
  pthread_mutex_lock(&engine->operation_lock);
  if(atomic_load(&engine->cancelled))
  {
    pthread_mutex_unlock(&engine->operation_lock);
    return engine_fail(error, size, DT_MOBILE_ERROR_CANCELLED, "operation cancelled");
  }
  pthread_mutex_lock(&engine_work_lock);
  sync_temperature_chroma(&engine->develop);
  return DT_MOBILE_OK;
}
static void end_operation(dt_mobile_engine *engine)
{
  pthread_mutex_unlock(&engine_work_lock);
  pthread_mutex_unlock(&engine->operation_lock);
}

dt_mobile_status dt_mobile_engine_modules(dt_mobile_engine *engine, char **json,
                                         char *error, size_t error_size)
{
  *json = NULL;
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  GString *description = g_string_new("[");
  gboolean first = TRUE;
  for(GList *item = engine->develop.iop; item; item = g_list_next(item))
  {
    dt_iop_module_t *module = item->data;
    char identifier[128];
    module_identifier(module, identifier, sizeof(identifier));
    if(!first) g_string_append_c(description, ',');
    first = FALSE;
    g_string_append(description, "{\"operation\":"); json_string(description, identifier);
    g_string_append(description, ",\"name\":");
    gchar *label = module->multi_name[0]
      ? g_strdup_printf("%s · %s", module->name(), module->multi_name) : g_strdup(module->name());
    json_string(description, label);
    g_free(label);
    g_string_append_printf(description, ",\"enabled\":%s,\"canDisable\":%s,\"parameters\":[",
                           module->enabled ? "true" : "false",
                           module->hide_enable_button ? "false" : "true");
    parameter_json fields = { description, TRUE };
    if(module->have_introspection && module->get_introspection())
      visit_fields(module, module->get_introspection()->field, 0, "", 0, append_parameter, &fields);
    g_string_append(description, "],\"curves\":[");
    append_curves(module, description);
    g_string_append_c(description, ']');
    append_rgb_curve_options(module, description);
    append_whitebalance(module, description);
    append_crop(module, description);
    g_string_append_c(description, '}');
  }
  g_string_append_c(description, ']');
  status = finish_json(description, json, error, error_size);
  end_operation(engine);
  return status;
}

static void save_module(dt_mobile_engine *engine, dt_iop_module_t *module, gboolean enable)
{
  /* no_image=TRUE avoids touching GTK viewports in this headless develop. */
  dt_dev_add_history_item_ext(&engine->develop, module, enable, TRUE);
  sync_temperature_chroma(&engine->develop);
  dt_dev_write_history(&engine->develop);
  record_history_state(engine);
}

dt_mobile_status dt_mobile_engine_history(dt_mobile_engine *engine, char **json,
                                         char *error, size_t error_size)
{
  *json = NULL;
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  const guint length = engine->history_states->len - 1;
  GString *description = g_string_new(NULL);
  g_string_append_printf(description,
    "{\"canUndo\":%s,\"canRedo\":%s,\"position\":%u,\"length\":%u,\"limit\":%u}",
    engine->history_position ? "true" : "false",
    engine->history_position < length ? "true" : "false",
    engine->history_position, length, MOBILE_HISTORY_LIMIT);
  status = finish_json(description, json, error, error_size);
  end_operation(engine);
  return status;
}

static dt_mobile_status move_history(dt_mobile_engine *engine, int direction,
                                     char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  const guint length = engine->history_states->len - 1;
  if((direction < 0 && !engine->history_position)
     || (direction > 0 && engine->history_position >= length))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                         direction < 0 ? "no earlier edit is available" : "no later edit is available");
  else
  {
    const guint target = direction < 0 ? engine->history_position - 1 : engine->history_position + 1;
    const mobile_history_state *state = g_ptr_array_index(engine->history_states, target);
    /* Clone before replacing the live list: each stored state remains owned by
     * the undo window, independently of the active develop history. */
    GList *history = dt_history_duplicate(state->history);
    GList *order = dt_ioppr_iop_order_copy_deep(state->iop_order);
    dt_develop_t *dev = &engine->develop;
    dt_pthread_mutex_lock(&dev->history_mutex);
    g_list_free_full(dev->history, dt_dev_free_history_item);
    g_list_free_full(dev->iop_order_list, free);
    dev->history = history;
    dev->iop_order_list = order;
    dev->iop_order_version = state->iop_order_version;
    /* Equal counts may still describe different snapshots. Suppress pop's
     * count-difference mask shortcut and restore the actual active forms once. */
    dev->history_end = state->history_end;
    dt_dev_pop_history_items_ext(dev, state->history_end);
    dev->chroma = state->chroma;
    sync_temperature_chroma(dev);
    GList *forms = NULL;
    guint index = 0;
    for(GList *item = dev->history; item && index < (guint)state->history_end; item = item->next, index++)
    {
      const dt_dev_history_item_t *entry = item->data;
      if(entry->forms) forms = entry->forms;
    }
    dt_masks_replace_current_forms(dev, forms);
    /* The desktop keeps retired forms alive for outstanding GUI references.
     * This private headless develop has none, and its independent history
     * copies own their forms. Release retirement copies between restores so
     * repeated mask undo/redo cannot grow beyond the bounded undo window. */
    g_list_free_full(dev->allforms, (GDestroyNotify)dt_masks_free_form);
    dev->allforms = NULL;
    dev->focus_hash = FALSE;
    dt_dev_pipe_synch_all(dev);
    dt_dev_invalidate_all(dev);
    /* Export/preview construct fresh upstream pipelines from this database;
     * the GUI-only reload/rebuild helpers must not run on a headless develop. */
    dt_dev_write_history(dev);
    dt_pthread_mutex_unlock(&dev->history_mutex);
    engine->history_position = target;
  }
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_undo(dt_mobile_engine *engine, char *error, size_t error_size)
{ return move_history(engine, -1, error, error_size); }

dt_mobile_status dt_mobile_engine_redo(dt_mobile_engine *engine, char *error, size_t error_size)
{ return move_history(engine, 1, error, error_size); }

dt_mobile_status dt_mobile_engine_set_curve(dt_mobile_engine *engine, const char *identifier,
                                           int channel, int interpolation,
                                           const dt_mobile_curve_point *points, size_t count,
                                           char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  dt_iop_module_t *module = find_module(engine, identifier);
  mobile_curve_layout layout;
  if(!curve_layout(module, &layout))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED, "this module has no supported curve editor");
  else if(channel < 0 || channel >= layout.editable_channels)
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED, "this curve channel is reserved or linked");
  else if(!valid_curve_points(points, count, interpolation))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT, "curve nodes or interpolation are invalid");
  else
  {
    /* RGB processing reads all three channel counts, even in linked mode.
     * Do not enable an imported module with malformed untouched channels. */
    for(int other = 0; other < layout.channels && status == DT_MOBILE_OK; other++)
      if(other != channel)
      {
        dt_mobile_curve_point original[MOBILE_CURVE_MAX_NODES];
        int original_count, original_interpolation;
        if(!read_curve(module, &layout, other, original, &original_count, &original_interpolation))
          status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED,
                               "an untouched curve channel requires a supported history state");
      }
    if(status == DT_MOBILE_OK)
    {
      char *params = module->params;
      for(size_t k = 0; k < count; k++)
      {
        char *node = params + layout.nodes + channel * layout.channel_stride + k * layout.node_stride;
        memcpy(node + layout.x, &points[k].x, sizeof(float));
        memcpy(node + layout.y, &points[k].y, sizeof(float));
      }
      const int nodes = (int)count;
      memcpy(params + layout.counts + channel * sizeof(int), &nodes, sizeof(nodes));
      memcpy(params + layout.types + channel * sizeof(int), &interpolation, sizeof(interpolation));
      save_module(engine, module, TRUE);
    }
  }
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_set_rgb_curve_options(dt_mobile_engine *engine,
                                                       const char *identifier, int mode,
                                                       int preserve_colors,
                                                       char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  dt_iop_module_t *module = find_module(engine, identifier);
  mobile_curve_layout curves;
  mobile_rgb_curve_options_layout options;
  if(!rgb_curve_options_layout(module, &curves, &options))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED,
                         "this module has no supported RGB curve options");
  else if(!enum_has_value(options.mode, mode) || !enum_has_value(options.preserve, preserve_colors))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                         "RGB curve mode or color-preservation choice is invalid");
  else
  {
    char *params = module->params;
    gboolean copy_channels = options.current_mode != mode && mode == 1;
    for(int channel = 1; channel < MOBILE_CURVE_CHANNELS && copy_channels; channel++)
    {
      dt_mobile_curve_point points[MOBILE_CURVE_MAX_NODES];
      int count = 0, interpolation = 0;
      /* The layout validation above has already checked every channel. */
      if(!read_curve(module, &curves, channel, points, &count, &interpolation)) copy_channels = FALSE;
      for(int node = 0; node < count && copy_channels; node++)
        if(points[node].x != points[node].y) copy_channels = FALSE;
    }
    if(copy_channels)
      for(int channel = 1; channel < MOBILE_CURVE_CHANNELS; channel++)
      {
        /* gui_changed in rgbcurve.c copies the entire fixed node row, then
         * count and interpolation, only when G and B are both identity. */
        memcpy(params + curves.nodes + channel * curves.channel_stride,
               params + curves.nodes, curves.channel_stride);
        memcpy(params + curves.counts + channel * sizeof(int), params + curves.counts, sizeof(int));
        memcpy(params + curves.types + channel * sizeof(int), params + curves.types, sizeof(int));
      }
    memcpy(params + options.mode->header.offset, &mode, sizeof(mode));
    memcpy(params + options.preserve->header.offset, &preserve_colors, sizeof(preserve_colors));
    save_module(engine, module, TRUE);
  }
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_set_whitebalance_gains(dt_mobile_engine *engine,
                                                       const char *identifier,
                                                       const float *gains, size_t count,
                                                       char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  dt_iop_module_t *module = find_module(engine, identifier);
  mobile_whitebalance_layout layout;
  if(!whitebalance_layout(module, &layout) || !whitebalance_available(module))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED,
                         "this module or monochrome image has no supported white-balance editor");
  else if(!gains || count != (size_t)layout.channels)
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                         "white-balance gains must match all active channels");
  else
  {
    for(size_t channel = 0; channel < count; channel++)
      if(!isfinite(gains[channel]) || gains[channel] < 0.001f || gains[channel] > 8.0f)
      {
        status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                             "manual white-balance gains must be within 0.001 to 8");
        break;
      }
    if(status == DT_MOBILE_OK)
    {
      for(size_t channel = 0; channel < count; channel++)
        memcpy((char *)module->params + layout.gains[channel], &gains[channel], sizeof(float));
      const int user_setting = 2;
      memcpy((char *)module->params + layout.preset, &user_setting, sizeof(user_setting));
      /* save_module also derives late_correction=FALSE and the enabled
       * temperature coefficients before capturing this single undo step. */
      save_module(engine, module, TRUE);
    }
  }
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_set_whitebalance_temperature(dt_mobile_engine *engine,
                                                             const char *identifier,
                                                             double kelvin, double tint,
                                                             char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  dt_iop_module_t *module = find_module(engine, identifier);
  mobile_whitebalance_layout layout;
  double XYZ_to_CAM[4][3], CAM_to_XYZ[3][4];
  if(!whitebalance_layout(module, &layout) || !whitebalance_available(module)
     || !whitebalance_matrices(module, XYZ_to_CAM, CAM_to_XYZ))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED,
                         "this image has no valid white-balance temperature calibration");
  else if(!isfinite(kelvin) || kelvin < DT_IOP_LOWEST_TEMPERATURE || kelvin > DT_IOP_HIGHEST_TEMPERATURE
          || !isfinite(tint) || tint < DT_IOP_LOWEST_TINT || tint > DT_IOP_HIGHEST_TINT)
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                         "white-balance temperature or tint is outside the desktop range");
  else
  {
    double multipliers[4];
    dt_whitebalance_XYZ_to_multipliers(XYZ_to_CAM,
      dt_whitebalance_temperature_tint_to_XYZ(kelvin, tint), multipliers);
    /* Exactly the desktop callback's green normalization. Validate all active
     * results before writing; unusual calibrations and extreme tints can yield
     * nonpositive camera channels. Never partially update the module. */
    const double green = multipliers[1];
    float gains[4];
    for(int channel = 0; channel < layout.channels; channel++)
    {
      const double normalized = channel == 1 ? 1.0 : multipliers[channel] / green;
      if(!isfinite(green) || green <= 0.0 || !isfinite(normalized)
         || normalized <= 0.0 || normalized > FLT_MAX
         || !isnormal((float)normalized))
      {
        status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                             "temperature and tint do not produce valid camera white-balance gains");
        break;
      }
      gains[channel] = (float)normalized;
    }
    if(status == DT_MOBILE_OK)
    {
      for(int channel = 0; channel < layout.channels; channel++)
        memcpy((char *)module->params + layout.gains[channel], &gains[channel], sizeof(float));
      const int user_setting = 2;
      memcpy((char *)module->params + layout.preset, &user_setting, sizeof(user_setting));
      save_module(engine, module, TRUE);
    }
  }
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_set_whitebalance_setting(dt_mobile_engine *engine,
                                                         const char *identifier, int setting,
                                                         char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  dt_iop_module_t *module = find_module(engine, identifier);
  mobile_whitebalance_layout layout;
  float gains[4];
  if(!whitebalance_layout(module, &layout) || !whitebalance_available(module))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED,
                         "this module or monochrome image has no supported white-balance editor");
  else if(setting != 0 && setting != 3 && setting != 4)
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                         "unknown built-in white-balance setting");
  else if(!whitebalance_basis(module, &layout, setting, gains))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED,
                         "this image has no valid native basis for the requested white-balance setting");
  else
  {
    for(int channel = 0; channel < layout.channels; channel++)
      memcpy((char *)module->params + layout.gains[channel], &gains[channel], sizeof(float));
    memcpy((char *)module->params + layout.preset, &setting, sizeof(setting));
    save_module(engine, module, TRUE);
  }
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_set_crop(dt_mobile_engine *engine, const char *identifier,
                                          float left, float top, float right, float bottom,
                                          char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  dt_iop_module_t *module = find_module(engine, identifier);
  mobile_crop_layout layout;
  if(!crop_layout(module, &layout))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED, "this module has no supported crop editor");
  else if(!dt_mobile_crop_valid(left, top, right, bottom))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT, "crop rectangle is invalid");
  else
  {
    const float coordinates[] = { left, top, right, bottom };
    const int freehand = 0;
    for(size_t k = 0; k < 4; k++)
      memcpy((char *)module->params + layout.coordinates[k], &coordinates[k], sizeof(float));
    /* src/iop/crop.c commit_params treats 0/0 as freehand. Keeping a previous
     * fixed ratio could alter the requested rectangle again during export. */
    for(size_t k = 0; k < 2; k++)
      memcpy((char *)module->params + layout.ratios[k], &freehand, sizeof(freehand));
    save_module(engine, module, TRUE);
  }
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_set_module_params(dt_mobile_engine *engine, const char *identifier,
                                                   const void *params, size_t size,
                                                   char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  dt_iop_module_t *module = find_module(engine, identifier);
  if(!module) status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED, "unknown processing module");
  else if(size != (size_t)module->params_size)
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                         "module parameter size does not match this engine version");
  else
  {
    memcpy(module->params, params, size);
    save_module(engine, module, TRUE);
  }
  end_operation(engine);
  return status;
}

typedef struct parameter_update { const char *name; double value; gboolean found, valid; } parameter_update;
static void update_parameter(dt_iop_module_t *module, const dt_introspection_field_t *field,
                             size_t offset, const char *path, void *data)
{
  parameter_update *update = data;
  if(strcmp(update->name, path)
     || !dt_mobile_parameter_allowed(module->op, path, field->header.type)) return;
  update->found = TRUE;
  void *destination = (char *)module->params + offset;
  long double current, minimum, maximum;
  const char *type;
  double value = update->value;
  if(!isfinite(value)) return;
  if(field->header.type == DT_INTROSPECTION_TYPE_FLOAT)
  {
    /* Validate the value that will actually be stored. A decimal such as 0.1
     * must compare to a float lower bound as the same float, rather than being
     * rejected because its double representation is slightly smaller. Reject
     * overflow before converting; integer/enum fields retain exact semantics. */
    if(value < -FLT_MAX || value > FLT_MAX) return;
    value = (double)(float)value;
    if(!isfinite(value)) return;
  }
  if(!number_info(field, destination, &current, &minimum, &maximum, &type)
     || (long double)value < minimum || (long double)value > maximum
     || (field->header.type != DT_INTROSPECTION_TYPE_FLOAT
         && field->header.type != DT_INTROSPECTION_TYPE_DOUBLE && trunc(value) != value)
     || !dt_mobile_parameter_value_valid(module, path, value)) return;
  if(field->header.type == DT_INTROSPECTION_TYPE_ENUM)
  {
    gboolean present = FALSE;
    for(size_t k = 0; k < enum_entry_count(field); k++)
      if(value == field->Enum.values[k].value) present = TRUE;
    if(!present) return;
  }
#define WRITE_NUMBER(tag, ctype) \
  case DT_INTROSPECTION_TYPE_##tag: *(ctype *)destination = (ctype)value; break
  switch(field->header.type)
  {
    WRITE_NUMBER(FLOAT, float);
    WRITE_NUMBER(DOUBLE, double);
    WRITE_NUMBER(CHAR, char);
    WRITE_NUMBER(INT8, int8_t);
    WRITE_NUMBER(UINT8, uint8_t);
    WRITE_NUMBER(SHORT, short);
    WRITE_NUMBER(USHORT, unsigned short);
    WRITE_NUMBER(INT, int);
    WRITE_NUMBER(UINT, unsigned int);
    WRITE_NUMBER(LONG, long);
    WRITE_NUMBER(ULONG, unsigned long);
    WRITE_NUMBER(BOOL, gboolean);
    WRITE_NUMBER(ENUM, int);
    default: return;
  }
#undef WRITE_NUMBER
  update->valid = TRUE;
}

dt_mobile_status dt_mobile_engine_set_parameter(dt_mobile_engine *engine, const char *identifier,
                                               const char *parameter, double value,
                                               char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  dt_iop_module_t *module = find_module(engine, identifier);
  parameter_update update = { parameter, value, FALSE, FALSE };
  if(module && module->have_introspection && module->get_introspection())
    visit_fields(module, module->get_introspection()->field, 0, "", 0, update_parameter, &update);
  if(!module || !update.found)
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED, "unknown module or numeric parameter");
  else if(!update.valid)
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT,
                         "parameter is outside its supported range, choices or invariants");
  else save_module(engine, module, TRUE);
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_set_enabled(dt_mobile_engine *engine, const char *identifier, int enabled,
                                             char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  dt_iop_module_t *module = find_module(engine, identifier);
  if(!module) status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED, "unknown processing module");
  else if(!strcmp(module->op, "temperature") && !whitebalance_available(module))
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_UNSUPPORTED,
                         "white balance is disabled for this true-monochrome image");
  else if(!enabled && module->hide_enable_button)
    status = engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT, "this pipeline module must remain enabled");
  else
  {
    module->enabled = enabled != 0;
    save_module(engine, module, FALSE);
  }
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_export_formats(dt_mobile_engine *engine, char **json,
                                                char *error, size_t error_size)
{
  *json = NULL;
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  GString *description = g_string_new("[");
  gboolean first = TRUE;
  for(GList *item = darktable.imageio->plugins_format; item; item = g_list_next(item))
  {
    dt_imageio_module_format_t *format = item->data;
    if(!strcmp(format->plugin_name, "copy")) continue;
    dt_imageio_module_data_t *params = format->get_params(format);
    if(!params) continue;
    if(!first) g_string_append_c(description, ',');
    first = FALSE;
    g_string_append(description, "{\"id\":"); json_string(description, format->plugin_name);
    g_string_append(description, ",\"name\":"); json_string(description, format->name());
    g_string_append(description, ",\"extension\":"); json_string(description, format->extension(params));
    g_string_append(description, ",\"mimeType\":"); json_string(description, format->mime(params));
    g_string_append_printf(description, ",\"bitDepth\":%d,\"options\":", format->bpp(params));
    dt_mobile_export_append_options(description, format->plugin_name);
    g_string_append_c(description, '}');
    format->free_params(format, params);
  }
  g_string_append_c(description, ']');
  status = finish_json(description, json, error, error_size);
  end_operation(engine);
  return status;
}

dt_mobile_status dt_mobile_engine_export(dt_mobile_engine *engine, const char *path,
                                        const char *format_name, int quality,
                                        char *error, size_t error_size)
{
  if(quality < 0 || quality > 100)
    return engine_fail(error, error_size, DT_MOBILE_ERROR_INVALID_ARGUMENT, "invalid export quality");
  dt_mobile_export_settings settings = DT_MOBILE_EXPORT_SETTINGS_INIT;
  /* The original API accepts 0..100, while the desktop JPEG encoder clamps
   * values below 5. Keep that behavior for its callers. */
  settings.quality = quality < 5 ? 5 : quality;
  return dt_mobile_engine_export_with_settings(engine, path, format_name, &settings, error, error_size);
}

dt_mobile_status dt_mobile_engine_export_with_settings(dt_mobile_engine *engine, const char *path,
                                                      const char *format,
                                                      const dt_mobile_export_settings *settings,
                                                      char *error, size_t error_size)
{
  dt_mobile_status status = begin_operation(engine, error, error_size);
  if(status != DT_MOBILE_OK) return status;
  status = dt_mobile_export_write(engine->image_id, path, format, settings,
                                   engine->preview_cancel, &engine->cancelled,
                                   error, error_size);
  end_operation(engine);
  return status;
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
  pthread_mutex_lock(&engine_work_lock);
  /* History snapshots borrow module pointers and must go before module cleanup. */
  g_clear_pointer(&engine->history_states, g_ptr_array_unref);
  dt_dev_cleanup(&engine->develop);
  pthread_mutex_unlock(&engine_work_lock);
  pthread_mutex_unlock(&engine->operation_lock);
  pthread_mutex_destroy(&engine->operation_lock);
  dt_imageio_preview_cancel_free(engine->preview_cancel);
  free(engine);
  pthread_mutex_lock(&runtime_lock);
  if(runtime_users && !--runtime_users) cleanup_runtime();
  pthread_mutex_unlock(&runtime_lock);
}
