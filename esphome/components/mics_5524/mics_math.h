#pragma once
//
// Pure math of the MiCS-5524 vendor model; per-gas constants live in `coefficients.py`.
//
// x = VCC - V_ao, ratio = x / x_air (the value sampled during the clean-air
// calibration), ppm = (threshold - ratio) / gain, per gas clamped to the vendor range.
//
// Free of ESPHome / ESP-IDF dependencies so the same code can be tested on the host.
//

#include <cmath>

namespace esphome::mics_5524::micsmath {

/// Per-gas constants of the vendor model, see `coefficients.py` for the table.
struct VendorCurve {
  float threshold;  ///< ratio at/below which the gas counts as detected
  float gain;       ///< ppm per unit ratio (the vendor's step size)
  float min_ppm;    ///< below this the vendor reports "0 ppm"
  float max_ppm;    ///< upper end of the measuring range
};

/// Vendor model input `x = VCC - V_ao` (NaN for unusable input).
///
/// Exactly 0 is V_AO at VCC, the saturated end of the vendor range, and stays 0 so that
/// a saturated sensor is not read as clean air.  A voltage *above* VCC cannot come from
/// the sensor, so it reports NaN instead of being folded into the saturated case: only
/// a wrong `vcc:` / `voltage_multiplier:` produces it.
inline float air_value_from_voltage(float voltage, float vcc) {
  if (!std::isfinite(voltage) || !std::isfinite(vcc))
    return NAN;
  const double x = static_cast<double>(vcc) - static_cast<double>(voltage);
  if (!std::isfinite(x) || x < 0.0)
    return NAN;
  return static_cast<float>(x);
}

/// Largest relative spread `(max - min) / mean` a clean-air calibration accepts.
inline constexpr float MAX_CALIBRATION_SPREAD = 0.25f;

/// True when the clean-air samples describe a settled input.
///
/// A calibration turns the mean of the samples into the reference every later reading is
/// divided by, so samples that disagree by more than `MAX_CALIBRATION_SPREAD` of their
/// mean show that the sensor was not in clean, still air: the average is worthless and
/// must not become the reference.  Unusable values are never a settled reference.
inline bool calibration_is_stable(float min_value, float max_value, float mean) {
  if (!std::isfinite(min_value) || !std::isfinite(max_value) || !std::isfinite(mean) || mean <= 0.0f)
    return false;
  const double spread = static_cast<double>(max_value) - static_cast<double>(min_value);
  return spread <= static_cast<double>(MAX_CALIBRATION_SPREAD) * static_cast<double>(mean);
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

}  // namespace esphome::mics_5524::micsmath
