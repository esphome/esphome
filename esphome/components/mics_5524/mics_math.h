#pragma once
//
// Pure math of the two MiCS-5524 conversion models; per-gas constants live in
// `coefficients.py`.
//
// "dfrobot"   - vendor model of the DFRobot_MICS library (MIT), which is what the
//               analog breakout is sold for: x = VCC - V_ao, ratio = x / x_air (the
//               value sampled during the clean-air calibration),
//               ppm = (threshold - ratio) / gain, clamped to the gas range.
// "datasheet" - log-log fit of the datasheet curve: RS = (VCC * RL) / V_ao - RL,
//               ratio = RS / R0 (R0 = RS in clean air), ppm = a * ratio^b.
//
// Every helper reports NaN for input that cannot produce a reading, so the caller
// publishes `unknown` instead of a made-up number.
//
// Free of ESPHome / ESP-IDF dependencies so the same code can be tested on the host.
//

#include <cfloat>
#include <cmath>
#include <cstdint>

namespace esphome::mics_5524::micsmath {

/// Conversion model of a sensor, mirrors `CONVERSION_MODELS` in `__init__.py`.
enum ConversionModel : uint8_t {
  CONVERSION_MODEL_DFROBOT = 0,    ///< vendor model of DFRobot_MICS (analog breakout)
  CONVERSION_MODEL_DATASHEET = 1,  ///< RS/R0 power law fitted on the datasheet curve
};

/// Per-gas constants of the vendor model, see `coefficients.py` for the table.
struct VendorCurve {
  float threshold;  ///< ratio at/below which the gas counts as detected
  float gain;       ///< ppm per unit ratio (the vendor's step size)
  float min_ppm;    ///< below this the vendor reports "0 ppm"
  float max_ppm;    ///< upper end of the measuring range
};

// ------------------------------------------------------------------ vendor model

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

/// Sensor resistance in kOhm for a divider output: RS = (VCC * RL) / V - RL.
///
/// NaN for a voltage that cannot come from the divider: a non-positive one would
/// give an infinite resistance and one at or above VCC a negative one, so a
/// saturated or mis-wired input is reported as `unknown` rather than as 0 kOhm.
inline float rs_from_voltage(float voltage, float vcc, float rl) {
  if (!std::isfinite(voltage) || !std::isfinite(vcc) || !std::isfinite(rl) || voltage <= 0.0f)
    return NAN;
  const double rs =
      static_cast<double>(vcc) * static_cast<double>(rl) / static_cast<double>(voltage) - static_cast<double>(rl);
  if (!std::isfinite(rs) || rs <= 0.0)
    return NAN;
  return static_cast<float>(rs);
}

/// Datasheet ratio RS / R0 (NaN when either value is unusable).
inline float ratio_from_rs(float rs, float r0) {
  if (!std::isfinite(rs) || rs <= 0.0f || !std::isfinite(r0) || r0 <= 0.0f)
    return NAN;
  const double ratio = static_cast<double>(rs) / static_cast<double>(r0);
  if (!std::isfinite(ratio))
    return NAN;
  return static_cast<float>(ratio);
}

/// Datasheet model: ppm = a * ratio^b (log-log fit of the datasheet curve).
///
/// NaN for a ratio or a coefficient that cannot produce a reading, so the caller
/// publishes `unknown` instead of the largest float there is.
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

/// Clamp a reading to the configured range (NaN stays NaN).
///
/// `min_ppm` is the noise floor of both models: a reading below it counts as not
/// detected and reports 0 ppm, the same way the vendor model reports 0 below its
/// own range. 0 ppm itself is never changed.
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

}  // namespace esphome::mics_5524::micsmath
