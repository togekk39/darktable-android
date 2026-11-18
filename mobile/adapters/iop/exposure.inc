/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included in the current upstream module; private types and algorithms stay upstream. */
#include <gmodule.h>
/* Use the same effective-exposure proxy without creating a desktop GUI or
 * consulting the global darkroom view. The caller selects its develop's
 * enabled exposure instance. */
G_MODULE_EXPORT int dt_iop_mobile_exposure_get_effective_exposure(dt_iop_module_t *self,
                                                                float *exposure)
{
  if(!self || !self->params || self->params_size != sizeof(dt_iop_exposure_params_t)
     || !self->dev || !self->enabled || !exposure) return 0;
  const dt_iop_exposure_params_t *const p = self->params;
  // deflicker has no effective-exposure cache outside the darkroom GUI
  if(p->mode == EXPOSURE_MODE_DEFLICKER && !self->gui_data) return 0;
  const float effective = _exposure_proxy_get_effective_exposure(self);
  if(!isfinite(effective)) return 0;
  *exposure = effective;
  return 1;
}
