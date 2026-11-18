/* SPDX-License-Identifier: GPL-3.0-or-later */
struct dt_imageio_preview_cancel_t
{
  GMutex mutex;
  dt_dev_pixelpipe_t *pipe;
  gboolean cancelled;
  dt_imageio_toneequal_sample_t *toneequal_sample;
};
