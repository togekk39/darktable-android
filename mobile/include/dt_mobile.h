/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DT_MOBILE_H
#define DT_MOBILE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct dt_mobile_session dt_mobile_session;
typedef enum dt_mobile_status {
  DT_MOBILE_OK = 0,
  DT_MOBILE_ERROR_INVALID_ARGUMENT = 1,
  DT_MOBILE_ERROR_IO = 2,
  DT_MOBILE_ERROR_UNSUPPORTED = 3,
  DT_MOBILE_ERROR_CANCELLED = 4,
  DT_MOBILE_ERROR_OUT_OF_MEMORY = 5
} dt_mobile_status;
dt_mobile_status dt_mobile_open(const char *path, dt_mobile_session **out_session);
const char *dt_mobile_open_error(void);
dt_mobile_status dt_mobile_set_module_params(dt_mobile_session *session, const char *module,
                                               const void *params, size_t params_size);
dt_mobile_status dt_mobile_render_preview(dt_mobile_session *session, int max_width, int max_height,
                                            uint8_t **rgba, int *width, int *height);
dt_mobile_status dt_mobile_export(dt_mobile_session *session, const char *output_path,
                                    const char *format, int quality);
void dt_mobile_free_buffer(void *buffer);
void dt_mobile_cancel(dt_mobile_session *session);
const char *dt_mobile_last_error(const dt_mobile_session *session);
void dt_mobile_close(dt_mobile_session *session);
#ifdef __cplusplus
}
#endif
#endif
