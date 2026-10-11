#pragma once
//
// Pure math of the MiCS-5524 gas sensor models.
//
// Two conversion models are implemented, because the published references use
// two different ones (see `coefficients.py` for the per-gas constants):
//
// 1) "dfrobot" - the vendor model of the DFRobot_MICS library (MIT), used by the
//    DFRobot/Fermion analog breakout and by the makerguides tutorial:
//
//        x     = VCC - V_ao                      (VCC = module supply, 5 V)
//        ratio = x / x_air                       (~1.0 in clean air, drops with gas)
//        ppm   = (threshold - ratio) / gain      per gas, clamped to its range
//
//    `x_air` is the value sampled during the clean-air calibration, so the model
//    neither needs the board's load resistor nor its divider ratio.
//
// 2) "datasheet" - the log-log fit of the datasheet curve, the same chain the
//    sibling mq_gas_sensors component uses:
//
//        RS    = (VCC * RL) / V_ao - RL
//        ratio = RS / R0                         (R0 = RS in clean air -> 1.0)
//        ppm   = a * ratio^b
//
// This header is intentionally free of ESPHome / ESP-IDF dependencies so the very
// same code can be unit tested on the host
// (tests/components/mics_5524_gas_sensor/mics_math_test.cpp).
//

#include <cfloat>
#include <cmath>
#include <cstdint>

namespace esphome::mics_5524_gas_sensor::micsmath {

/// Conversion model, mirrors `CONVERSION_MODELS` in `__init__.py`.
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
/// Returns 0 for a non-positive / non-finite voltage instead of +inf.
inline float rs_from_voltage(float voltage, float vcc, float rl) {
  if (!std::isfinite(voltage) || voltage <= 0.0f)
    return 0.0f;
  const double rs =
      (static_cast<double>(vcc) * static_cast<double>(rl) / static_cast<double>(voltage)) - static_cast<double>(rl);
  if (!std::isfinite(rs) || rs < 0.0)
    return 0.0f;
  return static_cast<float>(rs);
}

/// RS / R0 ratio (0 when either value is unusable).
inline float ratio_from_rs(float rs, float r0) {
  if (!std::isfinite(rs) || rs <= 0.0f || !std::isfinite(r0) || r0 <= 0.0f)
    return 0.0f;
  return static_cast<float>(static_cast<double>(rs) / static_cast<double>(r0));
}

/// The vendor model's raw quantity: how far the analog output sits below the
/// supply (`x = VCC - V_ao`). Clamped to >= 0, 0 for unusable input.
inline float air_value_from_voltage(float voltage, float vcc) {
  if (!std::isfinite(voltage) || !std::isfinite(vcc))
    return 0.0f;
  const double x = static_cast<double>(vcc) - static_cast<double>(voltage);
  if (!std::isfinite(x) || x <= 0.0)
    return 0.0f;
  return static_cast<float>(x);
}

/// ratio = x / x_air (0 when the calibration value is unusable).
inline float ratio_from_air_value(float x, float x_air) {
  if (!std::isfinite(x) || !std::isfinite(x_air) || x_air <= 0.0f)
    return 0.0f;
  const double ratio = static_cast<double>(x) / static_cast<double>(x_air);
  if (!std::isfinite(ratio) || ratio < 0.0)
    return 0.0f;
  return static_cast<float>(ratio);
}

/// Vendor model: ratio -> ppm for one gas (DFRobot_MICS `getGasData()`).
///
/// A ratio above the gas threshold means "not detected" and reports 0 ppm, a
/// value below the vendor's lower limit is reported as 0 ppm as well; the result
/// is clamped to [min_ppm, max_ppm]. NaN input stays NaN so the caller can
/// publish `unknown` instead of a fake 0.
inline float ppm_from_vendor_curve(float ratio, const VendorCurve &curve) {
  if (!std::isfinite(ratio))
    return NAN;
  if (ratio <= 0.0f || ratio > curve.threshold)
    return 0.0f;

  const double ppm =
      (static_cast<double>(curve.threshold) - static_cast<double>(ratio)) / static_cast<double>(curve.gain);
  if (!std::isfinite(ppm) || ppm < static_cast<double>(curve.min_ppm))
    return 0.0f;
  if (ppm > static_cast<double>(curve.max_ppm))
    return curve.max_ppm;
  return static_cast<float>(ppm);
}

/// Datasheet model: ppm = a * ratio^b (MQUnifiedsensor style regression).
/// FLT_MAX signals "out of range / not evaluable" so the caller can clamp it.
inline float ppm_from_power_law(float a, float b, float ratio) {
  if (!std::isfinite(ratio) || ratio <= 0.0f || a == 0.0f)
    return 0.0f;

  const double log_ppm =
      std::log10(static_cast<double>(a)) + static_cast<double>(b) * std::log10(static_cast<double>(ratio));
  double ppm;
  if (will_overflow(log_ppm)) {
    ppm = (std::isnan(log_ppm) || log_ppm > 0.0) ? static_cast<double>(FLT_MAX) : 0.0;
  } else {
    ppm = safe_pow(10.0, log_ppm);
  }
  if (std::isnan(ppm) || std::isinf(ppm))
    ppm = static_cast<double>(FLT_MAX);
  if (ppm < 0.0)
    ppm = 0.0;
  return static_cast<float>(ppm);
}

/// Clamp a PPM reading to the configured range (NaN stays NaN).
inline float clamp_ppm(float ppm, float min_ppm, float max_ppm) {
  if (!std::isfinite(ppm))
    return NAN;
  if (ppm < min_ppm)
    return min_ppm;
  if (ppm > max_ppm)
    return max_ppm;
  return ppm;
}

}  // namespace esphome::mics_5524_gas_sensor::micsmath
