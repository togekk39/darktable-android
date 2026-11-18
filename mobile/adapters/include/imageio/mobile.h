/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

/* Render the normal export pixelpipe into an owned, display-referred RGBA8
 * buffer.  This is the non-GUI counterpart of dt_imageio_preview(). */
typedef struct dt_imageio_preview_cancel_t dt_imageio_preview_cancel_t;

dt_imageio_preview_cancel_t *dt_imageio_preview_cancel_new(void);
void dt_imageio_preview_cancel(dt_imageio_preview_cancel_t *cancel);
void dt_imageio_preview_cancel_free(dt_imageio_preview_cancel_t *cancel);

/* Full-resolution export through the normal pixelpipe, with an optional
 * cancellation token shared with headless preview rendering. Uses the image's
 * output color profile and the format plugin's supplied encoding parameters. */
gboolean dt_imageio_export_to_file(const dt_imgid_t imgid,
                                  const char *filename,
                                  struct dt_imageio_module_format_t *format,
                                  struct dt_imageio_module_data_t *format_params,
                                  dt_export_metadata_t *metadata,
                                  dt_imageio_preview_cancel_t *cancel);

/* Same native pixelpipe and cancellation, with explicit export profile and
 * source-metadata policy. Scaling bounds come from format_params; upscaling
 * remains disabled. ignore_exif prevents the source blob reaching encoders. */
gboolean dt_imageio_export_to_file_with_options(const dt_imgid_t imgid,
                                  const char *filename,
                                  struct dt_imageio_module_format_t *format,
                                  struct dt_imageio_module_data_t *format_params,
                                  gboolean ignore_exif, gboolean copy_metadata,
                                  dt_colorspaces_color_profile_type_t icc_type,
                                  const gchar *icc_filename,
                                  dt_iop_color_intent_t icc_intent,
                                  dt_export_metadata_t *metadata,
                                  dt_imageio_preview_cancel_t *cancel);

gboolean dt_imageio_preview_to_memory(const dt_imgid_t imgid,
                                      const size_t max_width,
                                      const size_t max_height,
                                      uint8_t **rgba,
                                      uint32_t *width,
                                      uint32_t *height,
                                      dt_imageio_preview_cancel_t *cancel);

/* A synchronous sample of the selected toneequal instance's real filtered
 * input mask at normalized final-preview coordinates. No display RGB is used.
 * The request is scoped to the cancellation context's one original pipeline. */
typedef struct dt_imageio_toneequal_sample_t
{
  int multi_priority;
  double x, y;
  double exposure;
  gboolean found;
  float output_scale;
  int output_width, output_height;
} dt_imageio_toneequal_sample_t;

gboolean dt_imageio_toneequal_sample(const dt_imgid_t imgid,
                                      const size_t max_width,
                                      const size_t max_height,
                                      dt_imageio_toneequal_sample_t *sample,
                                      dt_imageio_preview_cancel_t *cancel);
