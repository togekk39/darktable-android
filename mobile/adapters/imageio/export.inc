/* SPDX-License-Identifier: GPL-3.0-or-later */
gboolean dt_imageio_export_with_flags(const dt_imgid_t imgid, const char *filename,
                                      dt_imageio_module_format_t *format,
                                      dt_imageio_module_data_t *format_params,
                                      const gboolean ignore_exif, const gboolean display_byteorder,
                                      const gboolean high_quality, const gboolean upscale,
                                      const gboolean is_scaling, const double scale_factor,
                                      const gboolean thumbnail_export, const char *filter,
                                      const gboolean copy_metadata, const gboolean export_masks,
                                      const dt_colorspaces_color_profile_type_t icc_type,
                                      const gchar *icc_filename, const dt_iop_color_intent_t icc_intent,
                                      dt_imageio_module_storage_t *storage,
                                      dt_imageio_module_data_t *storage_params, int num, const int total,
                                      dt_export_metadata_t *metadata, const int history_end)
{
  return _imageio_export_with_flags(imgid, filename, format, format_params, ignore_exif,
                                    display_byteorder, high_quality, upscale, is_scaling, scale_factor,
                                    thumbnail_export, filter, copy_metadata, export_masks, icc_type,
                                    icc_filename, icc_intent, storage, storage_params, num, total,
                                    metadata, history_end, NULL);
}
