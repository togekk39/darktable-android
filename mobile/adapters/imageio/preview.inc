/* SPDX-License-Identifier: GPL-3.0-or-later */
dt_imageio_preview_cancel_t *dt_imageio_preview_cancel_new(void)
{
  dt_imageio_preview_cancel_t *cancel = g_malloc0(sizeof(*cancel));
  g_mutex_init(&cancel->mutex);
  return cancel;
}

void dt_imageio_preview_cancel(dt_imageio_preview_cancel_t *cancel)
{
  if(!cancel) return;
  g_mutex_lock(&cancel->mutex);
  cancel->cancelled = TRUE;
  /* STOP_NODES is deliberately stored even while the pipe is idle: the
   * processing entry point preserves it and exits before running nodes. */
  if(cancel->pipe)
    dt_atomic_set_int(&cancel->pipe->shutdown, DT_DEV_PIXELPIPE_STOP_NODES);
  g_mutex_unlock(&cancel->mutex);
}

void dt_imageio_preview_cancel_free(dt_imageio_preview_cancel_t *cancel)
{
  if(!cancel) return;
  g_mutex_clear(&cancel->mutex);
  g_free(cancel);
}

gboolean dt_imageio_export_to_file(const dt_imgid_t imgid,
                                  const char *filename,
                                  dt_imageio_module_format_t *format,
                                  dt_imageio_module_data_t *format_params,
                                  dt_export_metadata_t *metadata,
                                  dt_imageio_preview_cancel_t *cancel)
{
  return dt_imageio_export_to_file_with_options(imgid, filename, format, format_params,
    FALSE, TRUE, DT_COLORSPACE_NONE, NULL, DT_INTENT_LAST, metadata, cancel);
}

gboolean dt_imageio_export_to_file_with_options(const dt_imgid_t imgid,
                                  const char *filename,
                                  dt_imageio_module_format_t *format,
                                  dt_imageio_module_data_t *format_params,
                                  gboolean ignore_exif, gboolean copy_metadata,
                                  dt_colorspaces_color_profile_type_t icc_type,
                                  const gchar *icc_filename,
                                  dt_iop_color_intent_t icc_intent,
                                  dt_export_metadata_t *metadata,
                                  dt_imageio_preview_cancel_t *cancel)
{
  if(!strcmp(format->mime(format_params), "x-copy"))
    return dt_imageio_export(imgid, filename, format, format_params,
                             TRUE, FALSE, FALSE, 1.0, copy_metadata, FALSE,
                             icc_type, icc_filename, icc_intent,
                             NULL, NULL, 1, 1, metadata);
  return _imageio_export_with_flags
    (imgid, filename, format, format_params, ignore_exif, FALSE, TRUE, FALSE,
     FALSE, 1.0, FALSE, NULL, copy_metadata, FALSE, icc_type, icc_filename,
     icc_intent, NULL, NULL, 1, 1, metadata, -1, cancel);
}

gboolean dt_imageio_preview_to_memory(const dt_imgid_t imgid,
                                      const size_t max_width,
                                      const size_t max_height,
                                      uint8_t **rgba,
                                      uint32_t *width,
                                      uint32_t *height,
                                      dt_imageio_preview_cancel_t *cancel)
{
  if(!rgba || !width || !height || !max_width || !max_height
     || max_width > UINT32_MAX || max_height > UINT32_MAX
     || max_width > SIZE_MAX / max_height
     || max_width * max_height > SIZE_MAX / sizeof(uint32_t))
    return TRUE;

  *rgba = NULL;
  *width = *height = 0;
  _imageio_preview_t dat = { 0 };
  dat.head.max_width = max_width;
  dat.head.max_height = max_height;
  dat.head.width = max_width;
  dat.head.height = max_height;
  dat.bpp = 8;
  dat.buf = dt_alloc_aligned(sizeof(uint32_t) * max_width * max_height);
  if(!dat.buf) return TRUE;

  dt_imageio_module_format_t format = { 0 };
  format.mime = _preview_mime;
  format.levels = _preview_levels;
  format.bpp = _preview_bpp;
  format.write_image = _preview_write_image;
  const gboolean failed = _imageio_export_with_flags
    (imgid, "mobile-preview", &format, (dt_imageio_module_data_t *)&dat,
     TRUE, TRUE, FALSE, FALSE, FALSE, 1.0, FALSE, NULL, FALSE, FALSE,
     DT_COLORSPACE_DISPLAY, NULL, DT_INTENT_LAST, NULL, NULL, 1, 1, NULL, -1, cancel);
  if(failed || !dat.width || !dat.height)
  {
    dt_free_align(dat.buf);
    return TRUE;
  }

  const size_t bytes = sizeof(uint32_t) * dat.width * dat.height;
  uint8_t *result = malloc(bytes);
  if(!result)
  {
    dt_free_align(dat.buf);
    return TRUE;
  }
  /* display_byteorder produces BGRx. The mobile API preserves that byte order
   * with opaque alpha; platform adapters must convert to their bitmap layout. */
  memcpy(result, dat.buf, bytes);
  for(size_t k = 3; k < bytes; k += 4) result[k] = 0xff;
  dt_free_align(dat.buf);
  *rgba = result;
  *width = dat.width;
  *height = dat.height;
  return FALSE;
}

gboolean dt_imageio_toneequal_sample(const dt_imgid_t imgid,
                                      const size_t max_width,
                                      const size_t max_height,
                                      dt_imageio_toneequal_sample_t *sample,
                                      dt_imageio_preview_cancel_t *cancel)
{
  if(!sample || !cancel) return TRUE;
  sample->found = FALSE;
  sample->exposure = NAN;
  g_mutex_lock(&cancel->mutex);
  if(cancel->pipe || cancel->toneequal_sample)
  {
    g_mutex_unlock(&cancel->mutex);
    return TRUE;
  }
  cancel->toneequal_sample = sample;
  g_mutex_unlock(&cancel->mutex);
  uint8_t *pixels = NULL;
  uint32_t width = 0, height = 0;
  const gboolean failed = dt_imageio_preview_to_memory(imgid, max_width, max_height,
                                                       &pixels, &width, &height, cancel);
  free(pixels);
  g_mutex_lock(&cancel->mutex);
  cancel->toneequal_sample = NULL;
  g_mutex_unlock(&cancel->mutex);
  return failed || !sample->found || !isfinite(sample->exposure);
}
