/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included in the current upstream module; private types and algorithms stay upstream. */
/* Native clients reuse the same upstream mask/form bookkeeping as GUI edits.
 * These hooks only touch original parameters; their owner records one history
 * step after all geometry and form settings have been validated. */
G_MODULE_EXPORT void dt_iop_mobile_sync_mask_forms(dt_iop_module_t *self)
{
  if(self && self->params && self->blend_params && self->dev
     && self->params_size == sizeof(dt_iop_retouch_params_t)) rt_resynch_params(self);
}

G_MODULE_EXPORT void dt_iop_mobile_retouch_add_form(dt_iop_module_t *self, int algorithm)
{
  if(!self || !self->params || !self->blend_params || !self->dev
     || self->params_size != sizeof(dt_iop_retouch_params_t)
     || algorithm < DT_IOP_RETOUCH_CLONE || algorithm > DT_IOP_RETOUCH_FILL) return;
  dt_iop_retouch_params_t *params = self->params;
  const dt_iop_retouch_algo_type_t previous = params->algorithm;
  params->algorithm = (dt_iop_retouch_algo_type_t)algorithm;
  rt_resynch_params(self);
  params->algorithm = previous;
}

/* Reset settings without converting non-clone blur/fill shapes into clone
 * operations. Their original mask geometry and algorithm family stay intact. */
G_MODULE_EXPORT void dt_iop_mobile_retouch_reset(dt_iop_module_t *self)
{
  if(!self || !self->params || !self->default_params || !self->dev || !self->blend_params
     || self->params_size != sizeof(dt_iop_retouch_params_t)) return;
  dt_iop_retouch_params_t *params = self->params;
  dt_iop_retouch_form_data_t previous[RETOUCH_NO_FORMS];
  memcpy(previous, params->rt_forms, sizeof(previous));
  memcpy(params, self->default_params, sizeof(*params));
  for(int index = 0; index < RETOUCH_NO_FORMS; index++)
  {
    dt_masks_form_t *form = dt_masks_get_from_id(self->dev, previous[index].formid);
    if(!form || !rt_get_mask_point_group(self, previous[index].formid)) continue;
    dt_iop_retouch_form_data_t *restored = &params->rt_forms[index];
    restored->formid = previous[index].formid;
    if(form->type & DT_MASKS_CLONE)
      restored->algorithm = previous[index].algorithm == DT_IOP_RETOUCH_CLONE
        ? DT_IOP_RETOUCH_CLONE : DT_IOP_RETOUCH_HEAL;
    else
      restored->algorithm = previous[index].algorithm == DT_IOP_RETOUCH_FILL
        ? DT_IOP_RETOUCH_FILL : DT_IOP_RETOUCH_BLUR;
    restored->scale = CLAMP(previous[index].scale, 0, params->num_scales + 1);
    restored->distort_mode = previous[index].distort_mode == 1 ? 1 : 2;
    restored->blur_type = params->blur_type;
    restored->blur_radius = params->blur_radius;
    restored->fill_mode = params->fill_mode;
    memcpy(restored->fill_color, params->fill_color, sizeof(restored->fill_color));
    restored->fill_brightness = params->fill_brightness;
  }
  rt_resynch_params(self);
}

static dt_iop_retouch_form_data_t *rt_mobile_form(dt_iop_module_t *self, const int32_t formid)
{
  if(!self || !self->params || !self->dev || self->params_size != sizeof(dt_iop_retouch_params_t)) return NULL;
  dt_iop_retouch_params_t *params = self->params;
  const int index = rt_get_index_from_formid(params, formid);
  if(index < 0 || !rt_get_mask_point_group(self, formid) || !dt_masks_get_from_id(self->dev, formid)) return NULL;
  return &params->rt_forms[index];
}

G_MODULE_EXPORT int dt_iop_mobile_retouch_get_form(dt_iop_module_t *self, int32_t formid,
    const char *name, double *value)
{
  dt_iop_retouch_form_data_t *form = rt_mobile_form(self, formid);
  if(!form || !name || !value) return -1;
  if(!strcmp(name, "max_scale")) *value = ((dt_iop_retouch_params_t *)self->params)->num_scales + 1;
  else if(!strcmp(name, "algorithm")) *value = form->algorithm;
  else if(!strcmp(name, "scale")) *value = form->scale;
  else if(!strcmp(name, "blur_type")) *value = form->blur_type;
  else if(!strcmp(name, "blur_radius")) *value = form->blur_radius;
  else if(!strcmp(name, "fill_mode")) *value = form->fill_mode;
  else if(!strcmp(name, "fill_brightness")) *value = form->fill_brightness;
  else if(!strcmp(name, "fill_color[0]")) *value = form->fill_color[0];
  else if(!strcmp(name, "fill_color[1]")) *value = form->fill_color[1];
  else if(!strcmp(name, "fill_color[2]")) *value = form->fill_color[2];
  else return 0;
  return isfinite(*value) ? 1 : -1;
}

G_MODULE_EXPORT int dt_iop_mobile_retouch_set_form(dt_iop_module_t *self, int32_t formid,
    const char *name, double value)
{
  dt_iop_retouch_form_data_t *form = rt_mobile_form(self, formid);
  if(!form || !name || !isfinite(value)) return -1;
  dt_iop_retouch_params_t *params = self->params;
  const float number = (float)value;
  if(!isfinite(number)) return -1;
  if(!strcmp(name, "algorithm"))
  {
    if(trunc(value) != value || value < DT_IOP_RETOUCH_CLONE || value > DT_IOP_RETOUCH_FILL) return -1;
    const int source_clone = form->algorithm == DT_IOP_RETOUCH_CLONE || form->algorithm == DT_IOP_RETOUCH_HEAL;
    const int target_clone = value == DT_IOP_RETOUCH_CLONE || value == DT_IOP_RETOUCH_HEAL;
    if(source_clone != target_clone) return -1; /* Original GUI clone/heal and blur/fill pairs. */
    if(value == DT_IOP_RETOUCH_BLUR
       && (!isfinite(form->blur_radius) || form->blur_radius < 0.1f || form->blur_radius > 200.f))
    {
      if(!isfinite(params->blur_radius) || params->blur_radius < 0.1f || params->blur_radius > 200.f
         || params->blur_type < DT_IOP_RETOUCH_BLUR_GAUSSIAN || params->blur_type > DT_IOP_RETOUCH_BLUR_BILATERAL) return -1;
      form->blur_radius = params->blur_radius;
      form->blur_type = params->blur_type;
    }
    form->algorithm = (dt_iop_retouch_algo_type_t)(int)value;
  }
  else if(!strcmp(name, "scale"))
  {
    if(trunc(value) != value || params->num_scales < 0 || params->num_scales > RETOUCH_MAX_SCALES
       || value < 0 || value > params->num_scales + 1) return -1;
    form->scale = (int)value;
  }
  else if(!strcmp(name, "blur_type"))
  {
    if(value != DT_IOP_RETOUCH_BLUR_GAUSSIAN && value != DT_IOP_RETOUCH_BLUR_BILATERAL) return -1;
    form->blur_type = (dt_iop_retouch_blur_types_t)(int)value;
  }
  else if(!strcmp(name, "blur_radius"))
  {
    if(number < 0.1f || number > 200.f) return -1;
    form->blur_radius = number;
  }
  else if(!strcmp(name, "fill_mode"))
  {
    if(value != DT_IOP_RETOUCH_FILL_ERASE && value != DT_IOP_RETOUCH_FILL_COLOR) return -1;
    form->fill_mode = (dt_iop_retouch_fill_modes_t)(int)value;
  }
  else if(!strcmp(name, "fill_brightness"))
  {
    if(number < -1.f || number > 1.f) return -1;
    form->fill_brightness = number;
  }
  else if(!strcmp(name, "fill_color[0]") || !strcmp(name, "fill_color[1]") || !strcmp(name, "fill_color[2]"))
  {
    if(number < 0.f || number > 1.f) return -1;
    form->fill_color[name[11] - '0'] = number;
  }
  else return 0;
  return 1;
}
