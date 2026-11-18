/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included only in the generated mobile module after the upstream solver. */
#include <float.h>
#include <gmodule.h>

static int _mobile_toneequal_adjust_params(dt_iop_toneequalizer_params_t *p,
                                            const double exposure, const double offset)
{
  if(!p || !isfinite(exposure)
     || fabs(exposure) > FLT_MAX || !isfinite(offset) || offset < -2 || offset > 2) return -1;
  if(offset == 0) return 0;
  dt_iop_toneequalizer_params_t next = *p;
  if(!isfinite(next.smoothing) || next.smoothing <= 0) return -1;
  dt_iop_toneequalizer_gui_data_t state = { 0 };
  state.sigma = next.smoothing;
  get_channels_factors(state.temp_user_params, &next);
  for(int index = 0; index < CHANNELS; index++)
    if(!isfinite(state.temp_user_params[index]) || state.temp_user_params[index] < .25f
       || state.temp_user_params[index] > 4) return -1;
  build_interpolation_matrix(state.interpolation_matrix, state.sigma);
  state.user_param_valid = TRUE;
  /* Processing clamps mask luminance to the two boundary bands. Centre the
   * touch edit on that same band: an HDR sample outside [-8,0] must not send
   * Gaussian weights towards zero and silently produce no visible edit. */
  const float control_exposure = fast_clamp((float)exposure, DT_TONEEQ_MIN_EV, DT_TONEEQ_MAX_EV);
  if(!set_new_params_interactive(control_exposure, (float)offset,
                                  state.sigma * state.sigma / 2.0f, &state, &next)) return -1;
  float gains[CHANNELS] DT_ALIGNED_ARRAY;
  get_channels_gains(gains, &next);
  for(int index = 0; index < CHANNELS; index++)
    if(!isfinite(gains[index]) || gains[index] < -2 || gains[index] > 2) return -1;
  if(!memcmp(p, &next, sizeof(next))) return 0;
  *p = next;
  return 1;
}

static int _mobile_toneequal_output_params(const dt_iop_toneequalizer_params_t *p,
                                            const double exposure, double *output_exposure)
{
  if(output_exposure) *output_exposure = NAN;
  if(!p || !output_exposure
     || !isfinite(exposure) || fabs(exposure) > FLT_MAX) return -1;
  if(!isfinite(p->smoothing) || p->smoothing <= 0) return -1;
  const float denominator = gaussian_denom(p->smoothing);
  if(!isfinite(denominator) || denominator <= 0) return -1;
  float factors[CHANNELS] DT_ALIGNED_ARRAY;
  get_channels_factors(factors, p);
  for(int index = 0; index < CHANNELS; index++)
    if(!isfinite(factors[index])) return -1;
  float interpolation[CHANNELS * PIXEL_CHAN] DT_ALIGNED_ARRAY;
  build_interpolation_matrix(interpolation, p->smoothing);
  if(!pseudo_solve(interpolation, factors, CHANNELS, PIXEL_CHAN, FALSE)) return -1;
  for(int index = 0; index < PIXEL_CHAN; index++)
    if(!isfinite(factors[index])) return -1;
  const float clamped = fast_clamp((float)exposure, DT_TONEEQ_MIN_EV, DT_TONEEQ_MAX_EV);
  const unsigned index = (unsigned)roundf((clamped - DT_TONEEQ_MIN_EV) * LUT_RESOLUTION);
  const float lookup_exposure = (float)index / (float)LUT_RESOLUTION + DT_TONEEQ_MIN_EV;
  const float correction = _correction_lut_entry(lookup_exposure, denominator, factors);
  if(!isfinite(correction) || correction <= 0) return -1;
  *output_exposure = exposure + (double)log2f(correction);
  return isfinite(*output_exposure) ? 1 : -1;
}

/* Run the desktop's grouped on-image edit without constructing GTK widgets.
 * All solver work and validation use a private copy; callers create exactly
 * one history item after this complete nine-channel change succeeds. */
G_MODULE_EXPORT int dt_iop_mobile_toneequal_adjust(dt_iop_module_t *self,
                                                   const double exposure,
                                                   const double offset)
{
  if(!self || !self->params || self->params_size != sizeof(dt_iop_toneequalizer_params_t) || !self->enabled) return -1;
  return _mobile_toneequal_adjust_params(self->params, exposure, offset);
}

/* Read-only output at a previously sampled input tone. The mask is an input
 * guide and does not change with the nine gains; the selected output does. */
G_MODULE_EXPORT int dt_iop_mobile_toneequal_output(dt_iop_module_t *self,
                                                   const double exposure,
                                                   double *output_exposure)
{
  if(output_exposure) *output_exposure = NAN;
  if(!self || !self->params || self->params_size != sizeof(dt_iop_toneequalizer_params_t) || !self->enabled) return -1;
  return _mobile_toneequal_output_params(self->params, exposure, output_exposure);
}

/* The touch slider requests an absolute selected output. Iteratively apply
 * the original desktop smooth update to a private parameter copy, comparing
 * its actual quantized processing curve with the target. A reachable target
 * is fitted within 0.002 EV; at a curve limit report the achieved output.
 * No intermediate solver pass becomes a native edit or history item. */
G_MODULE_EXPORT int dt_iop_mobile_toneequal_set_target(dt_iop_module_t *self,
                                                       const double exposure,
                                                       const double target,
                                                       double *output_exposure)
{
  if(output_exposure) *output_exposure = NAN;
  if(!self || !self->params || self->params_size != sizeof(dt_iop_toneequalizer_params_t)
     || !self->enabled || !output_exposure
     || !isfinite(exposure) || fabs(exposure) > FLT_MAX || !isfinite(target)
     || target < exposure - 2 || target > exposure + 2) return -1;
  dt_iop_toneequalizer_params_t best = *(const dt_iop_toneequalizer_params_t *)self->params;
  double output;
  if(_mobile_toneequal_output_params(&best, exposure, &output) < 0) return -1;
  double error = fabs(target - output);
  gboolean changed = FALSE;
  for(int iteration = 0; iteration < 12 && error > .002; iteration++)
  {
    double step = fmax(-2, fmin(2, target - output));
    gboolean improved = FALSE;
    for(int trial = 0; trial < 6; trial++, step *= .5)
    {
      dt_iop_toneequalizer_params_t candidate = best;
      const int result = _mobile_toneequal_adjust_params(&candidate, exposure, step);
      if(result < 0) return -1;
      if(!result) break;
      double candidate_output;
      if(_mobile_toneequal_output_params(&candidate, exposure, &candidate_output) < 0) return -1;
      const double candidate_error = fabs(target - candidate_output);
      if(candidate_error + 1e-7 < error)
      {
        best = candidate;
        output = candidate_output;
        error = candidate_error;
        improved = changed = TRUE;
        break;
      }
    }
    if(!improved) break;
  }
  *output_exposure = output;
  if(!changed) return 0;
  memcpy(self->params, &best, sizeof(best));
  return 1;
}
