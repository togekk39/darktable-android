/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included in the current upstream module; private types and algorithms stay upstream. */
#include <float.h>
#include <gmodule.h>
/* Parameter coupling shared with the headless editor. Keep the original
 * desktop sign convention for the signed black relative exposure. */
static void _adjust_source_grey(dt_iop_filmicrgb_params_t *p, const float previous)
{
  const float grey_var = log2f(previous / p->grey_point_source);
  p->black_point_source = p->black_point_source - grey_var;
  p->white_point_source = p->white_point_source + grey_var;
}

static void _rescale_source_range(dt_iop_filmicrgb_params_t *p, const float previous)
{
  const float ratio = (p->security_factor - previous) / (previous + 100.0f);
  p->black_point_source += ratio * p->black_point_source;
  p->white_point_source += ratio * p->white_point_source;
}

/* Unlike the pixel kernels, external numeric input can contain NaN/Inf. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("no-finite-math-only")
#endif
G_MODULE_EXPORT int dt_iop_mobile_editor_get_control(dt_iop_module_t *self, const char *name,
    double *value, double *minimum, double *maximum, double *original, int *kind)
{
  if(!self || !self->params || !self->default_params || !name || !value || !minimum
     || !maximum || !original || !kind || self->params_size != sizeof(dt_iop_filmicrgb_params_t)) return 0;
  const dt_iop_filmicrgb_params_t *p = self->params, *d = self->default_params;
  *kind = 0;
#define MOBILE_FILMIC_FLOAT(field, low, high) \
  if(!strcmp(name, #field)) { *value = p->field; *original = d->field; \
    *minimum = low; *maximum = high; return isfinite(*value); }
  MOBILE_FILMIC_FLOAT(grey_point_source, 0.001f, 100.f)
  MOBILE_FILMIC_FLOAT(black_point_source, -16.f, -0.1f)
  MOBILE_FILMIC_FLOAT(white_point_source, 0.1f, 16.f)
  MOBILE_FILMIC_FLOAT(security_factor, -50.f, 200.f)
#undef MOBILE_FILMIC_FLOAT
  if(strcmp(name, "auto_hardness")) return 0;
  *kind = 2; *value = p->auto_hardness; *original = d->auto_hardness;
  *minimum = 0; *maximum = 1;
  return 1;
}

static int _mobile_filmic_source_valid(const dt_iop_filmicrgb_params_t *p)
{
  return isfinite(p->grey_point_source) && p->grey_point_source >= 0.001f && p->grey_point_source <= 100.f
    && isfinite(p->black_point_source) && p->black_point_source >= -16.f && p->black_point_source <= -0.1f
    && isfinite(p->white_point_source) && p->white_point_source >= 0.1f && p->white_point_source <= 16.f
    && isfinite(p->security_factor) && p->security_factor >= -50.f && p->security_factor <= 200.f
    && isfinite(p->grey_point_target) && p->grey_point_target >= 1.f && p->grey_point_target <= 50.f;
}

G_MODULE_EXPORT int dt_iop_mobile_editor_set_number(dt_iop_module_t *self, const char *name, double value)
{
  if(!name) return -1;
  if(!strcmp(name, "output_power"))
  {
    if(!self || !self->params || self->params_size != sizeof(dt_iop_filmicrgb_params_t)) return -1;
    return ((dt_iop_filmicrgb_params_t *)self->params)->auto_hardness ? -1 : 0;
  }
  if(strcmp(name, "grey_point_source") && strcmp(name, "black_point_source")
     && strcmp(name, "white_point_source") && strcmp(name, "security_factor")
     && strcmp(name, "auto_hardness")) return 0;
  if(!isfinite(value) || !self || !self->so || !self->so->get_f
     || !self->so->get_f("output_power")) return -1;
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
  dt_iop_filmicrgb_params_t p = *(dt_iop_filmicrgb_params_t *)self->params;
  if(!strcmp(name, "grey_point_source"))
  {
    const float previous = p.grey_point_source;
    p.grey_point_source = value;
    _adjust_source_grey(&p, previous);
  }
  else if(!strcmp(name, "black_point_source")) p.black_point_source = value;
  else if(!strcmp(name, "white_point_source")) p.white_point_source = value;
  else if(!strcmp(name, "security_factor"))
  {
    const float previous = p.security_factor;
    p.security_factor = value;
    _rescale_source_range(&p, previous);
  }
  else p.auto_hardness = value;
  /* Reject a coupled range outside the original slider bounds atomically. */
  if(!_mobile_filmic_source_valid(&p)) return -1;
  if(p.auto_hardness) _compute_output_power(self, &p);
  if(!isfinite(p.output_power) || p.output_power < 1.f || p.output_power > 10.f) return -1;
  memcpy(self->params, &p, sizeof(p));
  return 1;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
