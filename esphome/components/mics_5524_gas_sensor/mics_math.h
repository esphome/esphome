#pragma once
//
// Pure math of the MiCS-5524 vendor model; per-gas constants live in `coefficients.py`.
//
// x = VCC - V_ao, ratio = x / x_air (the value sampled during the clean-air
// calibration), ppm = (threshold - ratio) / gain, per gas clamped to its range.
//
// Free of ESPHome / ESP-IDF dependencies so the same code can be tested on the host.
//

#include <cmath>

namespace esphome::mics_5524_gas_sensor::micsmath {

/// Per-gas constants of the vendor model, see `coefficients.py` for the table.
struct VendorCurve {
  float threshold;  ///< ratio at/below which the gas counts as detected
  float gain;       ///< ppm per unit ratio (the vendor's step size)
  float min_ppm;    ///< below this the vendor reports "0 ppm"
  float max_ppm;    ///< upper end of the measuring range
};

/// Vendor model input `x = VCC - V_ao` (0 at or above VCC, NaN for unusable input).
inline float air_value_from_voltage(float voltage, float vcc) {
  if (!std::isfinite(voltage) || !std::isfinite(vcc))
    return NAN;
  const double x = static_cast<double>(vcc) - static_cast<double>(voltage);
  if (!std::isfinite(x) || x <= 0.0)
    return 0.0f;
  return static_cast<float>(x);
}

/// ratio = x / x_air (NaN when either value is unusable).
inline float ratio_from_air_value(float x, float x_air) {
  if (!std::isfinite(x) || !std::isfinite(x_air) || x_air <= 0.0f)
    return NAN;
  const double ratio = static_cast<double>(x) / static_cast<double>(x_air);
  if (!std::isfinite(ratio) || ratio < 0.0)
    return NAN;
  return static_cast<float>(ratio);
}

/// Vendor model: ratio -> ppm for one gas (DFRobot_MICS `getGasData()`).
///
/// A ratio above the threshold means "not detected" -> 0 ppm, and so does a value
/// below the vendor's own measuring range.  A ratio of exactly 0 is V_AO at VCC,
/// the output of a saturated sensor: the vendor reports the top of the range
/// there, so it must never be read as clean air.  NaN stays NaN, so the caller
/// publishes `unknown`.
inline float ppm_from_vendor_curve(float ratio, const VendorCurve &curve) {
  if (!std::isfinite(ratio))
    return NAN;
  if (ratio > curve.threshold)
    return 0.0f;

  const double ppm =
      (static_cast<double>(curve.threshold) - static_cast<double>(ratio)) / static_cast<double>(curve.gain);
  if (!std::isfinite(ppm))
    return NAN;
  if (ppm < static_cast<double>(curve.min_ppm))
    return 0.0f;
  if (ppm > static_cast<double>(curve.max_ppm))
    return curve.max_ppm;
  return static_cast<float>(ppm);
}

/// Clamp a PPM reading to the configured range (NaN stays NaN).
///
/// 0 ppm is the vendor's way of saying "no gas detected" and is never lifted to
/// `min_ppm`; only a reading that the vendor counts as detected can be.
inline float clamp_ppm(float ppm, float min_ppm, float max_ppm) {
  if (!std::isfinite(ppm))
    return NAN;
  if (ppm <= 0.0f)
    return 0.0f;
  if (ppm < min_ppm)
    return min_ppm;
  if (ppm > max_ppm)
    return max_ppm;
  return ppm;
}

}  // namespace esphome::mics_5524_gas_sensor::micsmath
