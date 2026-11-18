/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included in the current upstream module; private types and algorithms stay upstream. */
/* Native single-image clients share the original mask-to-parameter sync.
 * The owning engine serializes access and records masks/history atomically. */
G_MODULE_EXPORT void dt_iop_mobile_sync_mask_forms(dt_iop_module_t *self)
{
  if(self && self->params && self->blend_params && self->dev
     && self->params_size == sizeof(dt_iop_spots_params_t)) _resynch_params(self);
}
