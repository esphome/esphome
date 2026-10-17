#pragma once
// Pure math of the MiCS-5524 vendor model; the per-gas constants live in coefficients.py.
// x = VCC - V_ao, ratio = x / x_air (sampled in clean air), ppm = (threshold - ratio) / gain.
// No ESPHome dependencies, so the host tests cover it.

#include <cmath>

namespace esphome::mics_5524::micsmath {

/// Per-gas constants of the vendor model.
struct VendorCurve {
  float threshold;  ///< ratio at or below which the gas counts as detected
  float gain;       ///< ppm per unit ratio
  float min_ppm;    ///< the vendor reports 0 ppm below this
  float max_ppm;    ///< upper end of the measuring range
};

/// x = VCC - V_ao: 0 at VCC (a saturated sensor, never clean air). NaN at or below 0 V (a
/// disconnected pin) and above VCC, which only a wrong vcc or voltage_multiplier produces.
inline float air_value_from_voltage(float voltage, float vcc) {
  if (!std::isfinite(voltage) || !std::isfinite(vcc) || voltage <= 0.0f || voltage > vcc)
    return NAN;
  return vcc - voltage;
}

/// Largest relative spread (max - min) / mean a clean-air calibration accepts.
inline constexpr float MAX_CALIBRATION_SPREAD = 0.25f;

/// True when the clean-air samples spread at most MAX_CALIBRATION_SPREAD of their mean.
inline bool calibration_is_stable(float min_value, float max_value, float mean) {
  if (!std::isfinite(min_value) || !std::isfinite(max_value) || !std::isfinite(mean) || mean <= 0.0f)
    return false;
  return max_value - min_value <= MAX_CALIBRATION_SPREAD * mean;
}

/// ratio = x / x_air (NaN when either value is unusable).
inline float ratio_from_air_value(float x, float x_air) {
  if (!std::isfinite(x) || x < 0.0f || !std::isfinite(x_air) || x_air <= 0.0f)
    return NAN;
  return x / x_air;
}

/// ratio -> ppm as DFRobot_MICS getGasData(): 0 above the threshold or below the vendor
/// range, the top of the range at ratio 0 (a saturated sensor), NaN stays NaN.
inline float ppm_from_vendor_curve(float ratio, const VendorCurve &curve) {
  if (!std::isfinite(ratio))
    return NAN;
  if (ratio > curve.threshold)
    return 0.0f;

  const float ppm = (curve.threshold - ratio) / curve.gain;
  if (ppm < curve.min_ppm)
    return 0.0f;
  if (ppm > curve.max_ppm)
    return curve.max_ppm;
  return ppm;
}

}  // namespace esphome::mics_5524::micsmath
