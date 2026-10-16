#pragma once
// Pure math of the two MiCS-5524 conversion models; the per-gas constants live in coefficients.py.
// dfrobot: x = VCC - V_ao, ratio = x / x_air (sampled in clean air), ppm = (threshold - ratio) / gain.
// datasheet: RS = (VCC * RL) / V_ao - RL, ratio = RS / R0 (RS in clean air), ppm = a * ratio^b.
// Every helper returns NaN for input that cannot produce a reading. No ESPHome dependencies, so the
// host tests cover it.

#include <cfloat>
#include <cmath>
#include <cstdint>

namespace esphome::mics_5524::micsmath {

/// Mirrors CONVERSION_MODELS in __init__.py.
enum ConversionModel : uint8_t {
  CONVERSION_MODEL_DFROBOT = 0,
  CONVERSION_MODEL_DATASHEET = 1,
};

/// Per-gas constants of the vendor model.
struct VendorCurve {
  float threshold;  ///< ratio at or below which the gas counts as detected
  float gain;       ///< ppm per unit ratio
  float min_ppm;    ///< the vendor reports 0 ppm below this
  float max_ppm;    ///< upper end of the measuring range
};

// ------------------------------------------------------------------ vendor model

/// x = VCC - V_ao: 0 at VCC (a saturated sensor, never clean air). NaN at or below 0 V (a
/// disconnected pin) and above VCC, which only a wrong vcc or voltage_multiplier produces.
inline float air_value_from_voltage(float voltage, float vcc) {
  if (!std::isfinite(voltage) || !std::isfinite(vcc) || voltage <= 0.0f || voltage > vcc)
    return NAN;
  return vcc - voltage;
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

// --------------------------------------------------------------- datasheet model

/// pow() with the fast paths of the reference implementations.
inline double safe_pow(double base, double exponent) {
  if (exponent == 0.0)
    return 1.0;
  if (exponent == 1.0)
    return base;
  if (exponent == 2.0)
    return base * base;
  return std::pow(base, exponent);
}

/// True when `log_ppm` cannot be turned into a finite float again.
inline bool will_overflow(double log_ppm) {
  const double max_log = std::log10(static_cast<double>(FLT_MAX));
  const double min_log = std::log10(static_cast<double>(FLT_MIN));
  return (log_ppm > max_log || log_ppm < min_log);
}

/// RS = (VCC * RL) / V - RL in kOhm; NaN at or below 0 V and at or above VCC, so a mis-wired
/// input reads unknown rather than 0 kOhm.
inline float rs_from_voltage(float voltage, float vcc, float rl) {
  if (!std::isfinite(voltage) || !std::isfinite(vcc) || !std::isfinite(rl) || voltage <= 0.0f)
    return NAN;
  // A voltage close to 0 V overflows the division.
  const float rs = vcc * rl / voltage - rl;
  return std::isfinite(rs) && rs > 0.0f ? rs : NAN;
}

/// ratio = RS / R0 (NaN when either value is unusable).
inline float ratio_from_rs(float rs, float r0) {
  if (!std::isfinite(rs) || rs <= 0.0f || !std::isfinite(r0) || r0 <= 0.0f)
    return NAN;
  return rs / r0;
}

/// ppm = a * ratio^b; NaN for a ratio or coefficient that cannot produce a reading.
inline float ppm_from_power_law(float a, float b, float ratio) {
  if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(ratio) || ratio <= 0.0f || a <= 0.0f)
    return NAN;

  const double log_ppm =
      std::log10(static_cast<double>(a)) + static_cast<double>(b) * std::log10(static_cast<double>(ratio));
  if (!std::isfinite(log_ppm) || will_overflow(log_ppm))
    return NAN;
  const double ppm = safe_pow(10.0, log_ppm);
  if (!std::isfinite(ppm) || ppm < 0.0)
    return NAN;
  return static_cast<float>(ppm);
}

/// Clamp to the configured range: below min_ppm counts as not detected and reads 0, and 0
/// itself never changes (NaN stays NaN).
inline float clamp_ppm(float ppm, float min_ppm, float max_ppm) {
  if (!std::isfinite(ppm))
    return NAN;
  if (ppm <= 0.0f)
    return 0.0f;
  if (std::isfinite(min_ppm) && ppm < min_ppm)
    return 0.0f;
  if (std::isfinite(max_ppm) && ppm > max_ppm)
    return max_ppm;
  return ppm;
}

// ----------------------------------------------------------------- calibration

/// Largest relative spread (max - min) / mean a clean-air calibration accepts.
inline constexpr float MAX_CALIBRATION_SPREAD = 0.25f;

/// True when the clean-air samples spread at most MAX_CALIBRATION_SPREAD of their mean.
inline bool calibration_is_stable(float min_value, float max_value, float mean) {
  if (!std::isfinite(min_value) || !std::isfinite(max_value) || !std::isfinite(mean) || mean <= 0.0f)
    return false;
  return max_value - min_value <= MAX_CALIBRATION_SPREAD * mean;
}

}  // namespace esphome::mics_5524::micsmath
