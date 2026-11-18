/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "common/whitebalance.h"
#include "whitebalance_upstream.inc"

cmsCIEXYZ dt_whitebalance_temperature_to_XYZ(double kelvin)
{
  return _temperature_to_XYZ(kelvin);
}

cmsCIEXYZ dt_whitebalance_temperature_tint_to_XYZ(double kelvin, double tint)
{
  return _temperature_tint_to_XYZ(kelvin, tint);
}

void dt_whitebalance_XYZ_to_temperature(cmsCIEXYZ xyz, float *kelvin, float *tint)
{
  _XYZ_to_temperature(xyz, kelvin, tint);
}

void dt_whitebalance_XYZ_to_multipliers(const double XYZ_to_CAM[4][3],
                                       cmsCIEXYZ xyz, double mul[4])
{
  dt_iop_temperature_gui_data_t gui = { 0 };
  memcpy(gui.XYZ_to_CAM, XYZ_to_CAM, sizeof(gui.XYZ_to_CAM));
  dt_iop_module_t module = { .gui_data = &gui };
  _xyz2mul(&module, xyz, mul);
}

cmsCIEXYZ dt_whitebalance_multipliers_to_XYZ(const double CAM_to_XYZ[3][4],
                                           const double mul[4])
{
  dt_iop_temperature_gui_data_t gui = { 0 };
  memcpy(gui.CAM_to_XYZ, CAM_to_XYZ, sizeof(gui.CAM_to_XYZ));
  dt_iop_module_t module = { .gui_data = &gui };
  const dt_iop_temperature_params_t params = { mul[0], mul[1], mul[2], mul[3] };
  return _mul2xyz(&module, &params);
}

void dt_whitebalance_srgb_matrices(double XYZ_to_CAM[4][3], double CAM_to_XYZ[3][4])
{
  _dt_mobile_upstream_srgb_matrices(XYZ_to_CAM, CAM_to_XYZ);
}
