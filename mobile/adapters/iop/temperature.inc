/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <float.h>

enum { DT_MOBILE_TEMPERATURE_FIRST_CAMERA_PRESET = 6 };
_Static_assert(DT_IOP_NUM_OF_STD_TEMP_PRESETS == 4, "review mobile temperature preset mapping");

/* Headless camera presets use the original catalog and settings-combo IDs.
 * The camera section heading follows the current upstream built-in modes.
 * Tuning is represented by coefficients in the native params, not by a new
 * persisted field. Keep this path independent of GTK and gui_data. */
typedef struct dt_mobile_temperature_group_t
{
  int first, last, untuned;
} dt_mobile_temperature_group_t;

static gboolean _mobile_temperature_available(const dt_iop_module_t *self)
{
  return self && self->params && self->params_size == sizeof(dt_iop_temperature_params_t)
    && self->dev && !dt_image_is_ldr(&self->dev->image_storage)
    && !(dt_image_monochrome_flags(&self->dev->image_storage) & DT_IMAGE_MONOCHROME);
}

static gboolean _mobile_temperature_camera(const dt_wb_data *wb, const dt_image_t *image)
{
  return wb && wb->make && wb->model && wb->name
    && !strcmp(wb->make, image->camera_maker) && !strcmp(wb->model, image->camera_model);
}

static int _mobile_temperature_groups(const dt_iop_module_t *self,
                                       dt_mobile_temperature_group_t groups[50])
{
  if(!_mobile_temperature_available(self)) return 0;
  const dt_image_t *image = &self->dev->image_storage;
  const int count = dt_wb_presets_count();
  int found = 0;
  for(int i = 0; i < count && found < 50; i++)
  {
    const dt_wb_data *wb = dt_wb_preset(i);
    if(!_mobile_temperature_camera(wb, image)) continue;
    dt_mobile_temperature_group_t *group = groups + found++;
    group->first = group->last = group->untuned = i;
    while(group->last + 1 < count)
    {
      const dt_wb_data *next = dt_wb_preset(group->last + 1);
      if(!_mobile_temperature_camera(next, image) || strcmp(wb->name, next->name)) break;
      group->last++;
      if(next->tuning == 0) group->untuned = group->last;
    }
    i = group->last;
  }
  return found;
}

static gboolean _mobile_temperature_valid_gains(const double gains[4], const int channels)
{
  for(int c = 0; c < channels; c++)
    if(!isfinite(gains[c]) || gains[c] <= 0.0 || gains[c] > FLT_MAX
       || !isnormal((float)gains[c])) return FALSE;
  return TRUE;
}

static gboolean _mobile_temperature_preset_gains(const dt_mobile_temperature_group_t *group,
                                                 const int tuning, const int channels,
                                                 double gains[4])
{
  for(int i = group->first; i <= group->last; i++)
  {
    const dt_wb_data *wb = dt_wb_preset(i);
    if(wb->tuning != tuning) continue;
    memcpy(gains, wb->channels, sizeof(wb->channels));
    return _mobile_temperature_valid_gains(gains, channels);
  }
  /* The upstream interpolator fills RGB only. Exact four-channel records are
   * supported, but inventing a fourth coefficient for a gap would be unsafe. */
  if(channels == 4) return FALSE;
  for(int i = group->first + 1; i <= group->last; i++)
  {
    const dt_wb_data *lower = dt_wb_preset(i - 1), *upper = dt_wb_preset(i);
    if(lower->tuning >= tuning || upper->tuning <= tuning) continue;
    if(!_mobile_temperature_valid_gains(lower->channels, channels)
       || !_mobile_temperature_valid_gains(upper->channels, channels)) return FALSE;
    dt_wb_data result = { .tuning = tuning };
    dt_wb_preset_interpolate(lower, upper, &result);
    memcpy(gains, result.channels, sizeof(result.channels));
    return _mobile_temperature_valid_gains(gains, channels);
  }
  return FALSE;
}

static void _mobile_temperature_json_string(GString *json, const char *value)
{
  g_string_append_c(json, '"');
  for(const unsigned char *c = (const unsigned char *)value; *c; c++)
  {
    if(*c == '"' || *c == '\\') g_string_append_c(json, '\\');
    if(*c < 0x20) g_string_append_printf(json, "\\u%04x", *c);
    else g_string_append_c(json, *c);
  }
  g_string_append_c(json, '"');
}

G_MODULE_EXPORT char *dt_iop_mobile_temperature_presets(dt_iop_module_t *self)
{
  dt_mobile_temperature_group_t groups[50];
  const int count = _mobile_temperature_groups(self, groups);
  if(!count) return NULL;
  const dt_image_t *image = &self->dev->image_storage;
  const int channels = image->flags & DT_IMAGE_4BAYER ? 4 : 3;
  const dt_iop_temperature_params_t *p = self->params;
  const float *current = (const float *)p;
  int selected = -1, selected_tuning = 0;
  GString *json = g_string_new("{\"camera\":");
  char *camera = g_strdup_printf("%s %s", image->camera_maker, image->camera_model);
  _mobile_temperature_json_string(json, camera);
  g_free(camera);
  g_string_append(json, ",\"presets\":[");
  gboolean first = TRUE;
  for(int j = 0; j < count; j++)
  {
    const dt_mobile_temperature_group_t *group = groups + j;
    const dt_wb_data *wb = dt_wb_preset(group->first);
    const int minimum = wb->tuning, maximum = dt_wb_preset(group->last)->tuning;
    int default_tuning = dt_wb_preset(group->untuned)->tuning;
    double gains[4];
    gboolean default_valid = _mobile_temperature_preset_gains(group, default_tuning, channels, gains);
    GString *tunings = g_string_new(NULL);
    for(int64_t tune = minimum; tune <= maximum; tune++)
    {
      if(!_mobile_temperature_preset_gains(group, (int)tune, channels, gains)) continue;
      if(!default_valid) { default_tuning = (int)tune; default_valid = TRUE; }
      if(tunings->len) g_string_append_c(tunings, ',');
      g_string_append_printf(tunings, "%d", (int)tune);
      if(p->preset == DT_IOP_NUM_OF_STD_TEMP_PRESETS + 1 + j && selected < 0)
      {
        gboolean equal = TRUE;
        for(int c = 0; c < channels; c++)
          if(current[c] != (float)gains[c]) { equal = FALSE; break; }
        if(equal) { selected = DT_MOBILE_TEMPERATURE_FIRST_CAMERA_PRESET + j; selected_tuning = (int)tune; }
      }
    }
    if(tunings->len)
    {
      if(!first) g_string_append_c(json, ',');
      first = FALSE;
      g_string_append_printf(json, "{\"id\":%d,\"label\":", DT_MOBILE_TEMPERATURE_FIRST_CAMERA_PRESET + j);
      _mobile_temperature_json_string(json, _(wb->name));
      g_string_append_printf(json, ",\"tuning\":[%s],\"defaultTuning\":%d}", tunings->str, default_tuning);
    }
    g_string_free(tunings, TRUE);
  }
  if(first) { g_string_free(json, TRUE); return NULL; }
  if(selected < 0) g_string_append(json, "],\"selectedPreset\":null,\"selectedTuning\":null}");
  else g_string_append_printf(json, "],\"selectedPreset\":%d,\"selectedTuning\":%d}", selected, selected_tuning);
  return g_string_free(json, FALSE);
}

G_MODULE_EXPORT int dt_iop_mobile_temperature_set_preset(dt_iop_module_t *self, int id, int tuning)
{
  if(id < DT_MOBILE_TEMPERATURE_FIRST_CAMERA_PRESET) return -1;
  dt_mobile_temperature_group_t groups[50];
  const int count = _mobile_temperature_groups(self, groups);
  const int group = id - DT_MOBILE_TEMPERATURE_FIRST_CAMERA_PRESET;
  if(group >= count) return -1;
  const int channels = self->dev->image_storage.flags & DT_IMAGE_4BAYER ? 4 : 3;
  double gains[4];
  if(!_mobile_temperature_preset_gains(groups + group, tuning, channels, gains)) return -1;
  dt_iop_temperature_params_t p;
  memcpy(&p, self->params, sizeof(p));
  float *coefficients = (float *)&p;
  for(int c = 0; c < channels; c++) coefficients[c] = (float)gains[c];
  p.preset = DT_IOP_NUM_OF_STD_TEMP_PRESETS + 1 + group;
  p.late_correction = FALSE;
  memcpy(self->params, &p, sizeof(p));
  return 1;
}

/* Publish exactly the state that the current upstream processor commits, so
 * headless history/default reloads use the same camera and late-correction
 * semantics as the desktop module. */
G_MODULE_EXPORT void dt_iop_mobile_temperature_sync(dt_iop_module_t *self)
{
  if(!self || !self->params || !self->dev
     || self->params_size != sizeof(dt_iop_temperature_params_t)) return;
  dt_dev_chroma_t *chroma = &self->dev->chroma;
  _wb_from_params(self->params, self->enabled, self->hide_enable_button,
                  chroma->as_shot, chroma->wb.D65coeffs, &chroma->wb);
  chroma->temperature = self;
}

/* Public mobile mode 4 retains its legacy meaning while native v5 stores that
 * choice as AS_SHOT plus late_correction. Public camera-preset IDs remain 6+
 * while the current native catalog starts at 5. */
G_MODULE_EXPORT int dt_iop_mobile_temperature_setting(dt_iop_module_t *self)
{
  if(!self || !self->params || !self->dev
     || self->params_size != sizeof(dt_iop_temperature_params_t)) return -1;
  const dt_iop_temperature_params_t *p = self->params;
  const dt_dev_chroma_t *chroma = &self->dev->chroma;
  const int mode = _params_mode(p, chroma->as_shot, chroma->wb.D65coeffs);
  if(mode >= DT_IOP_NUM_OF_STD_TEMP_PRESETS + 1)
    return DT_MOBILE_TEMPERATURE_FIRST_CAMERA_PRESET + mode - (DT_IOP_NUM_OF_STD_TEMP_PRESETS + 1);
  return mode == DT_IOP_TEMP_AS_SHOT && p->late_correction ? 4 : mode;
}
