/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included only in the generated mobile module before the CPU processor. */
#include "imageio/imageio_common.h"

/* Headless previews have no GTK luminance cache. Sample the same freshly
 * filtered mask while it still belongs to the original processor. Full-size
 * point transforms account for every geometric module after this instance;
 * ROI scale/origin then locate that point in the actual processed mask. */
static void _mobile_toneequal_sample(dt_iop_module_t *self,
                                      dt_dev_pixelpipe_iop_t *piece,
                                      const float *luminance,
                                      const dt_iop_roi_t *roi)
{
  dt_imageio_toneequal_sample_t *sample = piece->pipe->toneequal_sample;
  if(!sample || self->multi_priority != sample->multi_priority
     || sample->output_scale <= 0) return;
  /* A downstream OpenCL failure can restart the same pipe on the CPU. Always
   * replace the earlier pass, including invalidating an unusable final point. */
  sample->found = FALSE;
  sample->exposure = NAN;
  float point[2] = { sample->x * sample->output_width / sample->output_scale,
                     sample->y * sample->output_height / sample->output_scale };
  if(!dt_dev_distort_backtransform_plus(self->dev, piece->pipe, self->iop_order,
                                         DT_DEV_TRANSFORM_DIR_FORW_EXCL, point, 1)) return;
  const float x = point[0] * roi->scale - roi->x;
  const float y = point[1] * roi->scale - roi->y;
  if(!isfinite(x) || !isfinite(y) || x < 0 || y < 0 || x >= roi->width || y >= roi->height) return;
  const float value = get_luminance_from_buffer(luminance, roi->width, roi->height,
                                                (size_t)x, (size_t)y);
  if(isfinite(value) && value > 0)
  {
    sample->exposure = log2f(value);
    sample->found = isfinite(sample->exposure);
  }
}
