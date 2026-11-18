/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "zonemap_upstream.inc"

/* The interpolation is generated from the current upstream module. */
static inline void dt_zonesystem_calculate_zonemap(const float *zone, const int size, float *zonemap)
{
  dt_mobile_zonemap_source_params params = { .size = size, .zone = zone };
  _dt_mobile_upstream_zonemap(&params, zonemap);
}
