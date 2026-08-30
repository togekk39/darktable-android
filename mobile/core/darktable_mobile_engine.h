/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "dt_mobile.h"
typedef struct dt_mobile_engine dt_mobile_engine;
dt_mobile_status dt_mobile_engine_open(const char *path, dt_mobile_engine **engine,
                                        char *error, size_t error_size);
dt_mobile_status dt_mobile_engine_render(dt_mobile_engine *engine, int max_width, int max_height,
                                          uint8_t **pixels, int *width, int *height,
                                          char *error, size_t error_size);
void dt_mobile_engine_cancel(dt_mobile_engine *engine);
void dt_mobile_engine_close(dt_mobile_engine *engine);
