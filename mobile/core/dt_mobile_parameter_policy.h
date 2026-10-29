/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DT_MOBILE_PARAMETER_POLICY_H
#define DT_MOBILE_PARAMETER_POLICY_H

#include "common/introspection.h"
#include "develop/imageop.h"
#include <math.h>
#include <string.h>

/* Introspection describes storage, not the invariants maintained by a desktop
 * widget. In particular, curve counts, indexes and coordinates cannot safely be
 * edited independently, even when they have declared numeric bounds. New
 * controls must opt in here after their upstream processing code is reviewed.
 * Both metadata and the numeric setter use this policy; hiding a field in the
 * Android UI alone would leave unsafe writes reachable through the native API.
 *
 * The entries below are direct scalar fields, never structural counts/indexes:
 * - src/iop/exposure.c: params, commit_params and process use these as brightness
 *   coefficients/boolean compensation options, without changing buffer sizes.
 *   The value validator below also preserves its desktop GUI black/white relation.
 * - src/iop/velvia.c: params, commit_params and process use strength/bias only as
 *   saturation weights; the declared ranges match its desktop sliders.
 * - src/iop/colorcontrast.c: params, commit_params and process use the two
 *   steepness values only as Lab channel multipliers.
 * - src/iop/colorbalancergb.c: params and commit_params map these six independent
 *   grading coefficients to fixed-size processing data. They do not select a
 *   curve, mask, array element or allocation size.
 * - src/iop/sigmoid.c: contrast/skew produce fixed-size sigmoid coefficients
 *   with positive powers throughout their declared ranges. Hue preservation is
 *   clamped to [0, 1] after percentage conversion. Display targets, primaries
 *   and processing modes remain outside this reviewed subset.
 * - src/iop/sharpen.c: amount/threshold only weight the fixed unsharp-mask
 *   result. Radius remains private: arbitrary tiny positive values can make
 *   its Gaussian variance underflow, unlike the desktop widget's normal steps.
 * - src/iop/colisa.c: contrast/brightness/saturation use the original fixed-size
 *   LUTs and channel weights. Declared [-1,1] bounds keep brightness's gamma
 *   positive and bounded; these fields do not alter indexes or allocations.
 * - src/iop/colorize.c: the four original HSL/mix sliders feed the original
 *   HSL/XYZ/Lab conversion and a fixed-size pixel operation. The internal
 *   version selector remains private so imported legacy behavior is retained.
 *
 * Match the type as well as the name so an upstream schema change cannot turn
 * an approved float control into an unreviewed count or enum. Continue checking
 * upstream numeric bounds before writing; this policy does not replace them.
 */
static inline int dt_mobile_parameter_allowed(const char *operation, const char *parameter,
                                              dt_introspection_type_t type)
{
  static const struct {
    const char *operation;
    const char *parameter;
    dt_introspection_type_t type;
  } approved[] = {
    { "exposure", "exposure", DT_INTROSPECTION_TYPE_FLOAT },
    { "exposure", "black", DT_INTROSPECTION_TYPE_FLOAT },
    { "exposure", "compensate_exposure_bias", DT_INTROSPECTION_TYPE_BOOL },
    { "exposure", "compensate_hilite_pres", DT_INTROSPECTION_TYPE_BOOL },
    { "velvia", "strength", DT_INTROSPECTION_TYPE_FLOAT },
    { "velvia", "bias", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorcontrast", "a_steepness", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorcontrast", "b_steepness", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorbalancergb", "vibrance", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorbalancergb", "contrast", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorbalancergb", "chroma_global", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorbalancergb", "saturation_global", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorbalancergb", "brilliance_global", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorbalancergb", "hue_angle", DT_INTROSPECTION_TYPE_FLOAT },
    { "sigmoid", "middle_grey_contrast", DT_INTROSPECTION_TYPE_FLOAT },
    { "sigmoid", "contrast_skewness", DT_INTROSPECTION_TYPE_FLOAT },
    { "sigmoid", "hue_preservation", DT_INTROSPECTION_TYPE_FLOAT },
    { "sharpen", "amount", DT_INTROSPECTION_TYPE_FLOAT },
    { "sharpen", "threshold", DT_INTROSPECTION_TYPE_FLOAT },
    { "colisa", "contrast", DT_INTROSPECTION_TYPE_FLOAT },
    { "colisa", "brightness", DT_INTROSPECTION_TYPE_FLOAT },
    { "colisa", "saturation", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorize", "hue", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorize", "saturation", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorize", "source_lightness_mix", DT_INTROSPECTION_TYPE_FLOAT },
    { "colorize", "lightness", DT_INTROSPECTION_TYPE_FLOAT },
  };
  if(!operation || !parameter || strpbrk(parameter, "[.")) return 0;
  for(size_t k = 0; k < sizeof(approved) / sizeof(approved[0]); k++)
    if(type == approved[k].type && !strcmp(operation, approved[k].operation)
       && !strcmp(parameter, approved[k].parameter)) return 1;
  return 0;
}

static inline int dt_mobile_parameter_value_valid(dt_iop_module_t *module,
                                                  const char *parameter, double value)
{
  if(strcmp(module->op, "exposure")
     || (strcmp(parameter, "black") && strcmp(parameter, "exposure"))) return 1;
  if(!module->get_f || !module->get_p) return 0;
  const dt_introspection_field_t *black_field = module->get_f("black");
  const dt_introspection_field_t *exposure_field = module->get_f("exposure");
  if(!black_field || !exposure_field
     || black_field->header.type != DT_INTROSPECTION_TYPE_FLOAT
     || exposure_field->header.type != DT_INTROSPECTION_TYPE_FLOAT) return 0;
  const float *black_value = module->get_p(module->params, "black");
  const float *exposure_value = module->get_p(module->params, "exposure");
  if(!black_value || !exposure_value) return 0;
  const float black = !strcmp(parameter, "black") ? (float)value : *black_value;
  const float exposure = !strcmp(parameter, "exposure") ? (float)value : *exposure_value;
  /* src/iop/exposure.c gui_changed/_exposure_set_black/_exposure_set_white
   * maintain this relation, and _process_common_setup divides by white-black.
   * Reject a conflicting edit rather than silently changing a second control. */
  return isfinite(black) && isfinite(exposure) && black < exp2f(-exposure);
}

#endif
