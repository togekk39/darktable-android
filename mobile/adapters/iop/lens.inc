/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included in the current upstream module; private types and algorithms stay upstream. */
#include <float.h>
/* Headless document-editor bridge. Resolve the same image defaults as
 * gui_update before editing; setting has_been_set on an uninitialized history
 * blob otherwise freezes missing EXIF/profile values into every later commit.
 * No replacement correction algorithm is used here. */
static void _mobile_lens_normalize_auto(dt_iop_module_t *self, dt_iop_lens_params_t *p);

static dt_iop_lens_params_t _mobile_lens_effective(dt_iop_module_t *self)
{
  dt_iop_lens_params_t p = *(dt_iop_lens_params_t *)self->params;
  if(!p.has_been_set)
  {
    const dt_iop_lens_method_t method = p.method;
    p = *(dt_iop_lens_params_t *)self->default_params;
    p.method = _get_method(self, method);
    p.has_been_set = FALSE;
    p.camera[sizeof(p.camera) - 1] = '\0';
    p.lens[sizeof(p.lens) - 1] = '\0';
    _mobile_lens_normalize_auto(self, &p);
  }
  // match upstream's stored-parameter bounds before any database or JSON use
  p.camera[sizeof(p.camera) - 1] = '\0';
  p.lens[sizeof(p.lens) - 1] = '\0';
  return p;
}

static lfDatabase *_mobile_lens_db(dt_iop_module_t *self)
{
  const dt_iop_lens_global_data_t *gd = (const dt_iop_lens_global_data_t *)self->global_data;
  return gd ? gd->db : NULL;
}

static const lfCamera *_mobile_lens_camera(lfDatabase *db, const dt_iop_lens_params_t *p)
{
  const lfCamera *camera = NULL;
  if(db && p->camera[0])
  {
    const lfCamera **matches = db->FindCamerasExt(NULL, p->camera, 0);
    if(matches) camera = matches[0];
    lf_free(matches);
  }
  return camera;
}

static void _mobile_lens_normalize_auto(dt_iop_module_t *self, dt_iop_lens_params_t *p)
{
  if(p->has_been_set || p->method != DT_IOP_LENS_METHOD_LENSFUN) return;
  dt_pthread_mutex_lock(&darktable.plugin_threadsafe);
  const gboolean matched = _mobile_lens_camera(_mobile_lens_db(self), p) != NULL;
  dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);
  // EXIF may contain a generic/older lens name. Without a known body, that
  // string must not automatically select an unrelated database calibration.
  if(!matched) p->lens[0] = '\0';
}

G_MODULE_EXPORT int dt_iop_mobile_lens_prepare(dt_iop_module_t *self)
{
  if(!self || !self->params || !self->default_params || !self->dev) return 0;
  if(((const dt_iop_lens_params_t *)self->params)->has_been_set) return 0;
  dt_iop_lens_params_t p = _mobile_lens_effective(self);
  p.has_been_set = TRUE;
  memcpy(self->params, &p, sizeof(p));
  return 1;
}

static const lfLens *_mobile_lens_profile(lfDatabase *db, const lfCamera *camera,
                                        const dt_iop_lens_params_t *p)
{
  const lfLens *lens = NULL;
  if(db && p->lens[0])
  {
    const lfLens **matches = db->FindLenses(camera, NULL, p->lens, 0);
    if(matches) lens = matches[0];
    lf_free(matches);
  }
  return lens;
}

static gboolean _mobile_lens_profile_ready(const dt_iop_lens_params_t *p, const lfCamera *camera,
                                          const lfLens *lens)
{
  return lens && lens->Maker && isfinite(p->crop) && (camera ? camera->CropFactor : p->crop) > 0
    && isfinite(p->focal) && p->focal > 0 && isfinite(p->aperture) && p->aperture > 0
    && isfinite(p->distance) && p->distance > 0;
}

/* Report what the original DNG spline processor actually consumes. Three
 * planes can encode chromatic scale differences with an identity green plane;
 * that is not a fisheye/distortion calibration. Tangential terms [4..5] are
 * not consumed by the original radial spline and cannot advertise support. */
G_MODULE_EXPORT int dt_iop_mobile_lens_dng_capabilities(const dt_image_correction_data_t *cd)
{
  if(!cd) return 0;
  int supported = 0;
  if(cd->dng.has_warp && (cd->dng.planes == 1 || cd->dng.planes == 3))
  {
    gboolean finite = TRUE;
    for(int plane = 0; plane < cd->dng.planes; plane++)
      for(int term = 0; term < 4; term++)
        finite = finite && isfinite(cd->dng.cwarp[plane][term]);
    if(finite)
    {
      const int green = cd->dng.planes == 3 ? 1 : 0;
      for(int term = 0; term < 4; term++)
      {
        if(cd->dng.cwarp[green][term] != (term == 0 ? 1.0f : 0.0f))
          supported |= DT_IOP_LENS_MODIFY_FLAG_DISTORTION;
        if(cd->dng.planes == 3
           && (cd->dng.cwarp[0][term] != cd->dng.cwarp[green][term]
               || cd->dng.cwarp[2][term] != cd->dng.cwarp[green][term]))
          supported |= DT_IOP_LENS_MODIFY_FLAG_TCA;
      }
    }
  }
  if(cd->dng.has_vignette)
  {
    gboolean finite = TRUE, nonidentity = FALSE;
    for(int term = 0; term < 5; term++)
    {
      finite = finite && isfinite(cd->dng.cvig[term]);
      nonidentity = nonidentity || cd->dng.cvig[term] != 0.0f;
    }
    if(finite && nonidentity) supported |= DT_IOP_LENS_MODIFY_FLAG_VIGNETTING;
  }
  return supported;
}

/* Caller holds plugin_threadsafe, just like the normal Lensfun processor. */
static int _mobile_lens_supported(dt_iop_module_t *self, const dt_iop_lens_params_t *p,
                                 const lfCamera *camera, const lfLens *lens)
{
  int supported = 0;
  const dt_image_t *image = &self->dev->image_storage;
  if(p->method == DT_IOP_LENS_METHOD_EMBEDDED_METADATA && _have_embedded_metadata(self))
  {
    const gboolean dng = image->exif_correction_type == CORRECTION_TYPE_DNG;
    supported = dng ? dt_iop_mobile_lens_dng_capabilities(&image->exif_correction_data)
      : DT_IOP_LENS_MODFLAG_ALL;
  }
  else if(p->method == DT_IOP_LENS_METHOD_LENSFUN && _mobile_lens_profile_ready(p, camera, lens))
  {
    dt_iop_lens_data_t data = { 0 };
    data.modify_flags = DT_IOP_LENS_MODFLAG_ALL;
    data.lens = (lfLens *)lens; data.crop = camera ? camera->CropFactor : p->crop;
    data.focal = p->focal; data.aperture = p->aperture; data.distance = p->distance;
    data.scale = 1; data.inverse = p->inverse;
    data.target_geom = _lenstype_to_lensfun_lenstype(p->target_geom);
    int flags = 0;
    lfModifier *modifier = _get_modifier(&flags, image->width, image->height, &data, LF_MODIFY_ALL, FALSE);
    supported = _modflags_from_lensfun_mods(flags);
    delete modifier;
    if(p->tca_override) supported |= DT_IOP_LENS_MODIFY_FLAG_TCA;
  }
  if(dt_image_is_monochrome(image)) supported &= ~DT_IOP_LENS_MODIFY_FLAG_TCA;
  return supported;
}

G_MODULE_EXPORT int dt_iop_mobile_lens_choice_allowed(dt_iop_module_t *self, const char *name, int value)
{
  if(!self || !name || !self->params || !self->default_params || !self->dev) return 0;
  const dt_iop_lens_params_t p = _mobile_lens_effective(self);
  if(!strcmp(name, "method")) return value >= (_have_embedded_metadata(self) ? 0 : 1) && value <= 2;
  if(!strcmp(name, "inverse")) return value == 0 || value == 1;
  if(!strcmp(name, "target_geom")) return value >= 1 && value <= 8;
  if(strcmp(name, "modify_flags") || value < 0 || value > 7) return 0;
  dt_pthread_mutex_lock(&darktable.plugin_threadsafe);
  lfDatabase *db = _mobile_lens_db(self);
  const lfCamera *camera = _mobile_lens_camera(db, &p);
  const lfLens *lens = _mobile_lens_profile(db, camera, &p);
  const int supported = _mobile_lens_supported(self, &p, camera, lens);
  dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);
  if(!supported) return 0;
  if(value == DT_IOP_LENS_MODFLAG_ALL) return 1;
  if(value & ~supported) return 0;
  if(p.method == DT_IOP_LENS_METHOD_EMBEDDED_METADATA
     && self->dev->image_storage.exif_correction_type == CORRECTION_TYPE_DNG)
  {
    // The original processor applies the complete DNG warp when either of
    // these bits is selected. Do not offer independent operations it cannot do.
    const int warp = supported & (DT_IOP_LENS_MODIFY_FLAG_DISTORTION | DT_IOP_LENS_MODIFY_FLAG_TCA);
    const int selected = value & (DT_IOP_LENS_MODIFY_FLAG_DISTORTION | DT_IOP_LENS_MODIFY_FLAG_TCA);
    if(selected && selected != warp) return 0;
  }
  return 1;
}

/* kind: 0 float, 1 enum, 2 boolean, 3 action. This query is read-only and
 * returns effective image defaults, not a GUI's stale pre-initialization blob. */
G_MODULE_EXPORT int dt_iop_mobile_lens_get_control(dt_iop_module_t *self, const char *name,
    double *value, double *minimum, double *maximum, double *original, int *kind)
{
  if(!self || !self->params || !self->default_params || !self->dev || !name) return 0;
  const dt_iop_lens_params_t p = _mobile_lens_effective(self);
  const dt_iop_lens_params_t *d = (const dt_iop_lens_params_t *)self->default_params;
  *kind = 0;
  if(!strcmp(name, "method"))
  {
    *kind = 1; *value = p.method; *original = d->method;
    *minimum = _have_embedded_metadata(self) ? 0 : 1; *maximum = 2; return 1;
  }
  if(!strcmp(name, "lens.autodetect"))
  { *kind = 3; *value = *minimum = *maximum = 1; *original = NAN; return 1; }
#define MOBILE_LENS_FLOAT(field, low, high) \
  if(!strcmp(name, #field)) { *value = p.field; *original = d->field; *minimum = low; *maximum = high; return isfinite(*value); }
  MOBILE_LENS_FLOAT(v_strength, 0, 1)
  MOBILE_LENS_FLOAT(v_radius, 0, 1)
  MOBILE_LENS_FLOAT(v_steepness, 0, 1)
  const dt_image_t *image = &self->dev->image_storage;
  if(p.method == DT_IOP_LENS_METHOD_EMBEDDED_METADATA && _have_embedded_metadata(self))
  {
    if(!strcmp(name, "modify_flags"))
    {
      if(!_mobile_lens_supported(self, &p, NULL, NULL)) return 0;
      *kind = 1; *value = p.modify_flags; *original = d->modify_flags; *minimum = 0; *maximum = 7; return 1;
    }
    const gboolean dng = image->exif_correction_type == CORRECTION_TYPE_DNG;
    const int supported = _mobile_lens_supported(self, &p, NULL, NULL);
    if(!dng || (supported & (DT_IOP_LENS_MODIFY_FLAG_DISTORTION | DT_IOP_LENS_MODIFY_FLAG_TCA)))
    { MOBILE_LENS_FLOAT(cor_dist_ft, 0, 2) }
    if(!dng || (supported & DT_IOP_LENS_MODIFY_FLAG_VIGNETTING)) { MOBILE_LENS_FLOAT(cor_vig_ft, 0, 2) }
    if(!dng && p.md_version >= DT_IOP_LENS_EMBEDDED_METADATA_VERSION_2)
    { MOBILE_LENS_FLOAT(cor_ca_r_ft, 0, 2) MOBILE_LENS_FLOAT(cor_ca_b_ft, 0, 2) }
    MOBILE_LENS_FLOAT(scale_md, .1f, 2)
    return 0;
  }
  if(p.method != DT_IOP_LENS_METHOD_LENSFUN) return 0;
  dt_pthread_mutex_lock(&darktable.plugin_threadsafe);
  lfDatabase *db = _mobile_lens_db(self);
  const lfCamera *camera = _mobile_lens_camera(db, &p);
  const lfLens *lens = _mobile_lens_profile(db, camera, &p);
  const gboolean matched = lens && lens->Maker;
  const gboolean ready = _mobile_lens_profile_ready(&p, camera, lens);
  const int supported = !strcmp(name, "modify_flags") ? _mobile_lens_supported(self, &p, camera, lens) : 0;
  dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);
  if(!matched) return 0;
  if(!strcmp(name, "crop") && !camera)
  { *value = p.crop; *original = d->crop; *minimum = .1f; *maximum = 20; return isfinite(*value); }
  MOBILE_LENS_FLOAT(focal, .1f, 4000)
  MOBILE_LENS_FLOAT(aperture, .1f, 128)
  MOBILE_LENS_FLOAT(distance, .01f, 10000)
  if(!ready) return 0;
  if(!strcmp(name, "modify_flags"))
  {
    if(!supported) return 0;
    *kind = 1; *value = p.modify_flags; *original = d->modify_flags; *minimum = 0; *maximum = 7; return 1;
  }
  MOBILE_LENS_FLOAT(scale, .1f, 2)
  if(!strcmp(name, "lens.autoscale"))
  { *kind = 3; *value = *minimum = *maximum = 1; *original = NAN; return 1; }
  if(!strcmp(name, "inverse"))
  { *kind = 1; *value = p.inverse; *original = d->inverse; *minimum = 0; *maximum = 1; return 1; }
  if(!strcmp(name, "target_geom"))
  { *kind = 1; *value = p.target_geom; *original = d->target_geom; *minimum = 1; *maximum = 8; return 1; }
  if(!dt_image_is_monochrome(image))
  {
    if(!strcmp(name, "tca_override"))
    { *kind = 2; *value = p.tca_override; *original = d->tca_override; *minimum = 0; *maximum = 1; return 1; }
    if(p.tca_override) { MOBILE_LENS_FLOAT(tca_r, .99f, 1.01f) MOBILE_LENS_FLOAT(tca_b, .99f, 1.01f) }
  }
#undef MOBILE_LENS_FLOAT
  return 0;
}

G_MODULE_EXPORT int dt_iop_mobile_lens_set_number(dt_iop_module_t *self, const char *name, double value)
{
  if(!self || !name || !isfinite(value)) return -1;
  dt_iop_lens_params_t p = _mobile_lens_effective(self);
  double current, minimum, maximum, original;
  int kind;
  {
    if(!dt_iop_mobile_lens_get_control(self, name, &current, &minimum, &maximum, &original, &kind)) return -1;
    if(kind != 0 && value != trunc(value)) return -1;
    if(kind == 0)
    {
      if(value < -FLT_MAX || value > FLT_MAX) return -1;
      value = (double)(float)value;
    }
    if(value < minimum || value > maximum) return -1;
    if(kind == 1 && !dt_iop_mobile_lens_choice_allowed(self, name, (int)value)) return -1;
    if(!strcmp(name, "lens.autodetect"))
    {
      p = *(dt_iop_lens_params_t *)self->default_params;
      p.has_been_set = FALSE;
      _mobile_lens_normalize_auto(self, &p);
    }
    else if(!strcmp(name, "lens.autoscale"))
    {
      dt_pthread_mutex_lock(&darktable.plugin_threadsafe);
      const lfCamera *camera = _mobile_lens_camera(_mobile_lens_db(self), &p);
      dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);
      const float scale = _get_autoscale_lf(self, &p, camera);
      if(!isfinite(scale) || scale < .1f || scale > 2) return -1;
      p.scale = scale;
    }
    else if(!strcmp(name, "method"))
    {
      if(p.method != value && value == DT_IOP_LENS_METHOD_LENSFUN)
      {
        p.method = DT_IOP_LENS_METHOD_LENSFUN;
        p.has_been_set = FALSE;
        _mobile_lens_normalize_auto(self, &p);
      }
      else p.method = (dt_iop_lens_method_t)(int)value;
    }
    else if(!strcmp(name, "modify_flags")) p.modify_flags = (dt_iop_lens_modflag_t)(int)value;
    else if(!strcmp(name, "inverse")) p.inverse = (dt_iop_lens_mode_t)(int)value;
    else if(!strcmp(name, "target_geom")) p.target_geom = (dt_iop_lens_lenstype_t)(int)value;
    else if(!strcmp(name, "tca_override")) p.tca_override = value != 0;
#define MOBILE_LENS_SET(field) else if(!strcmp(name, #field)) p.field = (float)value;
    MOBILE_LENS_SET(scale) MOBILE_LENS_SET(crop) MOBILE_LENS_SET(focal) MOBILE_LENS_SET(aperture)
    MOBILE_LENS_SET(distance) MOBILE_LENS_SET(tca_r) MOBILE_LENS_SET(tca_b)
    MOBILE_LENS_SET(cor_dist_ft) MOBILE_LENS_SET(cor_vig_ft) MOBILE_LENS_SET(cor_ca_r_ft)
    MOBILE_LENS_SET(cor_ca_b_ft) MOBILE_LENS_SET(scale_md) MOBILE_LENS_SET(v_strength)
    MOBILE_LENS_SET(v_radius) MOBILE_LENS_SET(v_steepness)
#undef MOBILE_LENS_SET
    else return -1;
  }
  p.has_been_set = TRUE;
  memcpy(self->params, &p, sizeof(p));
  return 1;
}

G_MODULE_EXPORT int dt_iop_mobile_lens_set_text(dt_iop_module_t *self, const char *name, const char *value)
{
  if(!self || !name || !value || strlen(value) >= 128 || !g_utf8_validate(value, -1, NULL)) return -1;
  dt_iop_lens_params_t p = _mobile_lens_effective(self);
  const gboolean camera = !strcmp(name, "camera") || !strcmp(name, "lens.camera_profile");
  const gboolean lens = !strcmp(name, "lens") || !strcmp(name, "lens.profile");
  if(!camera && !lens) return -1;
  const gboolean canonical = !strncmp(name, "lens.", 5);
  if(canonical && (p.method != DT_IOP_LENS_METHOD_LENSFUN || !value[0])) return -1;
  dt_pthread_mutex_lock(&darktable.plugin_threadsafe);
  lfDatabase *db = _mobile_lens_db(self);
  if(camera)
  {
    g_strlcpy(p.camera, value, sizeof(p.camera));
    const lfCamera *match = _mobile_lens_camera(db, &p);
    if(canonical && (!match || strcmp(match->Model, value)))
    { dt_pthread_mutex_unlock(&darktable.plugin_threadsafe); return -1; }
    if(match) p.crop = match->CropFactor;
  }
  else
  {
    g_strlcpy(p.lens, value, sizeof(p.lens));
    const lfCamera *body = _mobile_lens_camera(db, &p);
    // The normal profile picker is a camera-compatible selector. An unknown
    // EXIF model must not unlock every lens in the database. The legacy text
    // API still preserves explicit advanced/manual settings in old histories.
    if(canonical && !body)
    { dt_pthread_mutex_unlock(&darktable.plugin_threadsafe); return -1; }
    const lfLens *match = _mobile_lens_profile(db, body, &p);
    if(canonical && (!match || strcmp(match->Model, value)))
    { dt_pthread_mutex_unlock(&darktable.plugin_threadsafe); return -1; }
    if(match)
    {
      p.target_geom = _lenstype_from_lensfun_lenstype(match->Type);
      if(!isfinite(p.focal) || p.focal <= 0 || match->MinFocal >= match->MaxFocal) p.focal = match->MinFocal;
      if(!isfinite(p.aperture) || p.aperture <= 0) p.aperture = match->MinAperture > 0 ? match->MinAperture : 4;
      if(!isfinite(p.distance) || p.distance <= 0) p.distance = 1000;
      if(!isfinite(p.crop) || p.crop <= 0) p.crop = body ? body->CropFactor : match->CropFactor;
    }
  }
  dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);
  p.has_been_set = TRUE;
  memcpy(self->params, &p, sizeof(p));
  return 1;
}

static void _mobile_lens_json_string(GString *json, const char *value)
{
  g_string_append_c(json, '"');
  for(const unsigned char *c = (const unsigned char *)(value ? value : ""); *c; c++)
  {
    if(*c == '"' || *c == '\\') g_string_append_c(json, '\\');
    if(*c < 0x20) g_string_append_printf(json, "\\u%04x", *c);
    else g_string_append_c(json, *c);
  }
  g_string_append_c(json, '"');
}

static void _mobile_lens_catalog_item(GString *json, const char *model, const char *maker,
                                      GHashTable *seen, gboolean *first)
{
  if(!model || strlen(model) >= 128 || !g_utf8_validate(model, -1, NULL)
     || g_hash_table_contains(seen, model)) return;
  g_hash_table_add(seen, (gpointer)model);
  if(!*first) g_string_append_c(json, ',');
  *first = FALSE;
  g_string_append(json, "{\"value\":"); _mobile_lens_json_string(json, model);
  g_string_append(json, ",\"label\":");
  gchar *label = maker && maker[0] ? g_strdup_printf("%s · %s", maker, model) : g_strdup(model);
  _mobile_lens_json_string(json, label); g_free(label);
  g_string_append_c(json, '}');
}

G_MODULE_EXPORT char *dt_iop_mobile_lens_metadata(dt_iop_module_t *self)
{
  if(!self || !self->params || !self->default_params || !self->dev) return NULL;
  const dt_iop_lens_params_t p = _mobile_lens_effective(self);
  dt_pthread_mutex_lock(&darktable.plugin_threadsafe);
  lfDatabase *db = _mobile_lens_db(self);
  const lfCamera *const *cameras = db ? db->GetCameras() : NULL;
  const lfLens *const *lenses = db ? db->GetLenses() : NULL;
  const gboolean database = cameras && cameras[0] && lenses && lenses[0];
  const lfCamera *camera = _mobile_lens_camera(db, &p);
  const lfLens *lens = _mobile_lens_profile(db, camera, &p);
  const gboolean ready = _mobile_lens_profile_ready(&p, camera, lens);
  const gboolean embedded = _have_embedded_metadata(self);
  const int supported = _mobile_lens_supported(self, &p, camera, lens);
  const char *status = p.method == DT_IOP_LENS_METHOD_ONLYVIGNETTE ? "manual"
    : p.method == DT_IOP_LENS_METHOD_EMBEDDED_METADATA ? (supported ? "ready" : "no_calibration")
    : !database ? "missing_database" : !camera ? (lens ? "unverified_camera" : "missing_camera")
    : !lens ? "missing_lens" : !ready ? "missing_settings"
    : supported ? "ready" : "no_calibration";
  GString *json = g_string_new("{\"status\":"); _mobile_lens_json_string(json, status);
  g_string_append_printf(json, ",\"method\":%d,\"embeddedAvailable\":%s,\"databaseAvailable\":%s,"
    "\"cameraMatched\":%s,\"lensMatched\":%s,\"supportedFlags\":%d,\"camera\":",
    p.method, embedded ? "true" : "false", database ? "true" : "false",
    camera ? "true" : "false", lens ? "true" : "false", supported);
  // These are verified database selections; source EXIF/requested strings are
  // diagnostics, not evidence that a calibration exists for that camera.
  _mobile_lens_json_string(json, camera ? camera->Model : "");
  g_string_append(json, ",\"lens\":"); _mobile_lens_json_string(json, lens ? lens->Model : "");
  g_string_append(json, ",\"requestedCamera\":"); _mobile_lens_json_string(json, p.camera);
  g_string_append(json, ",\"requestedLens\":"); _mobile_lens_json_string(json, p.lens);
  g_string_append(json, ",\"imageCamera\":"); _mobile_lens_json_string(json, self->dev->image_storage.exif_model);
  g_string_append(json, ",\"imageLens\":"); _mobile_lens_json_string(json, self->dev->image_storage.exif_lens);
  g_string_append(json, ",\"cameras\":[");
  GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
  gboolean first = TRUE;
  if(self->enabled && p.method == DT_IOP_LENS_METHOD_LENSFUN && cameras)
    for(unsigned index = 0; cameras[index]; index++)
      _mobile_lens_catalog_item(json, cameras[index]->Model, lf_mlstr_get(cameras[index]->Maker), seen, &first);
  g_hash_table_remove_all(seen);
  g_string_append(json, "],\"lenses\":["); first = TRUE;
  const lfLens **compatible = self->enabled && p.method == DT_IOP_LENS_METHOD_LENSFUN && db && camera
    ? db->FindLenses(camera, NULL, NULL, 0) : NULL;
  const lfLens *const *catalog = compatible;
  if(self->enabled && p.method == DT_IOP_LENS_METHOD_LENSFUN && catalog)
    for(unsigned index = 0; catalog[index]; index++)
      _mobile_lens_catalog_item(json, catalog[index]->Model, lf_mlstr_get(catalog[index]->Maker), seen, &first);
  lf_free(compatible);
  g_hash_table_destroy(seen);
  dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);
  g_string_append(json, "]}");
  return g_string_free(json, FALSE);
}
