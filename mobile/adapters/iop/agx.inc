/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included in the current upstream module; private types and algorithms stay upstream. */
#include "develop/blend.h"
#include <float.h>
#include <gmodule.h>
#include <limits.h>
/* Resolve the desktop preference within this editor's develop, rather than
 * darktable.develop or the current view. A masked later instance must not
 * silently override the first enabled unmasked exposure instance. */
static int _mobile_agx_effective_exposure(dt_iop_module_t *self, float *exposure)
{
  if(!self || !self->dev || !exposure) return 0;
  dt_iop_module_t *preferred = NULL;
  gboolean preferred_unmasked = FALSE;
  for(GList *item = self->dev->iop; item; item = g_list_next(item))
  {
    dt_iop_module_t *candidate = item->data;
    if(!candidate || candidate->dev != self->dev || strcmp(candidate->op, "exposure") || !candidate->enabled
       || candidate->iop_order == INT_MAX || !candidate->module || !candidate->params) continue;
    const gboolean unmasked = !candidate->blend_params
      || candidate->blend_params->mask_mode == DEVELOP_MASK_DISABLED
      || candidate->blend_params->mask_mode == DEVELOP_MASK_ENABLED;
    if(!preferred || (unmasked && !preferred_unmasked))
    {
      preferred = candidate;
      preferred_unmasked = unmasked;
    }
  }
  if(!preferred) return 0;
  typedef int (*effective_function)(dt_iop_module_t *, float *);
  effective_function get = NULL;
  if(!g_module_symbol(preferred->module, "dt_iop_mobile_exposure_get_effective_exposure", (gpointer *)&get)
     || !get) return 0;
  return get(preferred, exposure) == 1 && isfinite(*exposure);
}

/* kind: 0 float, 2 boolean, 3 action. Values retain the original parameter
 * units used by the mobile scalar API. Queries never update params/history. */
G_MODULE_EXPORT int dt_iop_mobile_editor_get_control(dt_iop_module_t *self, const char *name,
    double *value, double *minimum, double *maximum, double *original, int *kind)
{
  if(!self || !self->params || !self->default_params || !name || !value || !minimum
     || !maximum || !original || !kind || self->params_size != sizeof(dt_iop_agx_params_t)) return 0;
  const dt_iop_agx_params_t *p = self->params, *d = self->default_params;
  *kind = 0;
#define MOBILE_AGX_FLOAT(field, low, high) \
  if(!strcmp(name, #field)) { *value = p->field; *original = d->field; \
    *minimum = low; *maximum = high; return isfinite(*value); }
  MOBILE_AGX_FLOAT(range_black_relative_ev, -20.f, -0.1f)
  MOBILE_AGX_FLOAT(range_white_relative_ev, 0.1f, 20.f)
  MOBILE_AGX_FLOAT(dynamic_range_scaling, -0.5f, 2.f)
  MOBILE_AGX_FLOAT(curve_pivot_x, 0.f, 1.f)
  MOBILE_AGX_FLOAT(curve_pivot_y_linear_output, 0.f, 1.f)
#undef MOBILE_AGX_FLOAT
  if(!strcmp(name, "auto_gamma"))
  { *kind = 2; *value = p->auto_gamma; *original = d->auto_gamma; *minimum = 0; *maximum = 1; return 1; }
  if(!strcmp(name, "agx.range_from_exposure"))
  {
    float exposure;
    if(!_mobile_agx_effective_exposure(self, &exposure)) return 0;
  }
  else if(!strcmp(name, "agx.copy_pre_to_post"))
  {
    if(p->disable_primaries_adjustments || p->completely_reverse_primaries) return 0;
  }
  else if(strcmp(name, "agx.primaries_blender") && strcmp(name, "agx.primaries_smooth")
          && strcmp(name, "agx.primaries_unmodified")) return 0;
  *kind = 3; *value = *minimum = *maximum = 1; *original = NAN;
  return 1;
}

static int _mobile_agx_coupled_valid(const dt_iop_agx_params_t *p)
{
  if(!isfinite(p->range_black_relative_ev) || p->range_black_relative_ev < -20.f
     || p->range_black_relative_ev > -0.1f || !isfinite(p->range_white_relative_ev)
     || p->range_white_relative_ev < 0.1f || p->range_white_relative_ev > 20.f
     || !isfinite(p->dynamic_range_scaling) || p->dynamic_range_scaling < -0.5f
     || p->dynamic_range_scaling > 2.f || !isfinite(p->curve_pivot_x)
     || p->curve_pivot_x < 0.f || p->curve_pivot_x > 1.f
     || !isfinite(p->curve_pivot_y_linear_output)
     || !(p->curve_target_display_black_ratio < p->curve_pivot_y_linear_output
          && p->curve_pivot_y_linear_output < p->curve_target_display_white_ratio)
     || !isfinite(p->curve_gamma) || p->curve_gamma < 0.01f || p->curve_gamma > 100.f) return 0;
  return 1;
}

/* Apply original desktop coupling to a local copy. The adapter owns the one
 * history item and pipeline invalidation after this complete batch succeeds. */
G_MODULE_EXPORT int dt_iop_mobile_editor_set_number(dt_iop_module_t *self, const char *name, double value)
{
  if(!name) return -1;
  if(!strcmp(name, "curve_gamma"))
  {
    if(!self || !self->params || self->params_size != sizeof(dt_iop_agx_params_t)) return -1;
    return ((dt_iop_agx_params_t *)self->params)->auto_gamma ? -1 : 0;
  }
  const gboolean owned = !strcmp(name, "range_black_relative_ev") || !strcmp(name, "range_white_relative_ev")
    || !strcmp(name, "dynamic_range_scaling") || !strcmp(name, "curve_pivot_x")
    || !strcmp(name, "curve_pivot_y_linear_output") || !strcmp(name, "auto_gamma")
    || !strncmp(name, "agx.", 4);
  if(!owned) return 0;
  if(!isfinite(value)) return -1;
  double current, minimum, maximum, original;
  int kind;
  if(!dt_iop_mobile_editor_get_control(self, name, &current, &minimum, &maximum, &original, &kind)) return -1;
  if(kind == 0)
  {
    if(value < -FLT_MAX || value > FLT_MAX) return -1;
    value = (double)(float)value;
  }
  else if(value != trunc(value)) return -1;
  if(value < minimum || value > maximum) return -1;
  dt_iop_agx_params_t p = *(dt_iop_agx_params_t *)self->params;
  const float old_black = p.range_black_relative_ev, old_white = p.range_white_relative_ev;
  if(!strcmp(name, "range_black_relative_ev"))
  { p.range_black_relative_ev = value; _update_pivot_x(old_black, old_white, &p); }
  else if(!strcmp(name, "range_white_relative_ev"))
  { p.range_white_relative_ev = value; _update_pivot_x(old_black, old_white, &p); }
  else if(!strcmp(name, "dynamic_range_scaling"))
  {
    const float previous = p.dynamic_range_scaling;
    p.dynamic_range_scaling = value;
    const float ratio = (p.dynamic_range_scaling - previous) / (previous + 1.f);
    p.range_black_relative_ev = old_black * (1.f + ratio);
    p.range_white_relative_ev = old_white * (1.f + ratio);
    _update_pivot_x(old_black, old_white, &p);
  }
  else if(!strcmp(name, "curve_pivot_x")) p.curve_pivot_x = value;
  else if(!strcmp(name, "curve_pivot_y_linear_output")) p.curve_pivot_y_linear_output = value;
  else if(!strcmp(name, "auto_gamma")) p.auto_gamma = value;
  else if(!strcmp(name, "agx.range_from_exposure"))
  {
    float exposure;
    if(!_mobile_agx_effective_exposure(self, &exposure)) return -1;
    _adjust_relative_exposure(exposure, &p);
  }
  else if(!strcmp(name, "agx.primaries_blender")) _set_blenderlike_primaries(&p);
  else if(!strcmp(name, "agx.primaries_smooth")) _set_smooth_primaries(&p);
  else if(!strcmp(name, "agx.primaries_unmodified")) _set_unmodified_primaries(&p);
  else if(!strcmp(name, "agx.copy_pre_to_post")) _set_post_curve_primaries_from_pre(&p);
  else return -1;
  if(p.auto_gamma)
  {
    tone_mapping_params_t curve = { 0 };
    _set_log_mapping_params(&p, &curve);
    _adjust_pivot(&p, &curve);
    if(!isfinite(curve.curve_gamma)) return -1;
    p.curve_gamma = CLAMPF(curve.curve_gamma, 0.01f, 100.f);
  }
  if(!_mobile_agx_coupled_valid(&p)) return -1;
  memcpy(self->params, &p, sizeof(p));
  return 1;
}
