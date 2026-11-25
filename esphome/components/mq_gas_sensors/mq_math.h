#pragma once
//
// Pure math of the MQ sensor PPM model (port of MQUnifiedsensor.cpp):
// RS = (VCC * RL) / V - RL, ratio = RS / R0, PPM = a * ratio^b.
//
// Free of ESPHome / ESP-IDF dependencies so the same code runs on the host.
//

#include <cfloat>
#include <cmath>
#include <cstdint>

namespace esphome::mq_gas_sensors {

/// Regression model, mirrors MQUnifiedsensor's `_regressionMethod`.
enum class RegressionMethod : uint8_t {
  REGRESSION_METHOD_EXPONENTIAL = 1,  ///< _PPM = a * ratio^b
  REGRESSION_METHOD_LINEAR = 2,       ///< log10(_PPM) = (log10(ratio) - b) / a
  REGRESSION_METHOD_INVERSE = 3,      ///< _PPM = (ratio / a)^(1 / b)
};

namespace mqmath {

inline double safe_pow(double base, double exponent) {
  if (exponent == 0.0)
    return 1.0;
  if (exponent == 1.0)
    return base;
  if (exponent == 2.0)
    return base * base;
  return std::pow(base, exponent);
}

inline bool will_overflow(double log_ppm) {
  const double max_log = std::log10(static_cast<double>(FLT_MAX));
  const double min_log = std::log10(static_cast<double>(FLT_MIN));
  return (log_ppm > max_log || log_ppm < min_log);
}

/// Sensor resistance in kOhm, MQUnifiedsensor::getRS() / calibrate().
inline float rs_from_voltage(float voltage, float vcc, float rl) {
  if (!std::isfinite(voltage) || voltage <= 0.0f)
    return NAN;
  const double rs =
      (static_cast<double>(vcc) * static_cast<double>(rl) / static_cast<double>(voltage)) - static_cast<double>(rl);
  if (!std::isfinite(rs) || rs < 0.0)
    return NAN;
  return static_cast<float>(rs);
}

/// RS / R0 ratio, MQUnifiedsensor::readSensorR0Rs().
/// An unusable input reads NaN (the caller publishes `unknown`); only RS = 0
/// keeps the "0 means saturated" meaning `ppm_from_ratio()` reads as the top of
/// the measuring range.
inline float ratio_from_rs(float rs, float r0) {
  if (!std::isfinite(rs) || rs < 0.0f)
    return NAN;
  if (rs == 0.0f)
    return 0.0f;
  if (!std::isfinite(r0) || r0 <= 0.0f)
    return NAN;
  const double ratio = static_cast<double>(rs) / static_cast<double>(r0);
  if (!std::isfinite(ratio) || ratio <= 0.0)
    return NAN;
  return static_cast<float>(ratio);
}

/// Limit of a curve as the ratio falls to 0. A curve that grows as the ratio
/// shrinks (the datasheet direction) approaches FLT_MAX, one that falls
/// approaches 0, and one the coefficients cannot evaluate returns NaN. The caller
/// clamps FLT_MAX to its maximum.
inline float ppm_at_zero(float a, float b, RegressionMethod method) {
  switch (method) {
    case RegressionMethod::REGRESSION_METHOD_EXPONENTIAL:
      if (!(a > 0.0f))
        return NAN;
      if (b > 0.0f)
        return 0.0f;
      if (b < 0.0f)
        return FLT_MAX;
      return a;
    case RegressionMethod::REGRESSION_METHOD_INVERSE:
      if (!(a > 0.0f) || b == 0.0f)
        return NAN;
      return b > 0.0f ? 0.0f : FLT_MAX;
    case RegressionMethod::REGRESSION_METHOD_LINEAR:
      if (a == 0.0f)
        return NAN;
      return a > 0.0f ? 0.0f : FLT_MAX;
  }
  return NAN;
}

/// ratio (+ a, b) -> PPM, MQUnifiedsensor::readSensorR0Rs().
/// FLT_MAX signals "above the range" so the caller can clamp it. A ratio of 0 is
/// the limit of the curve, not automatically the top of the range: a custom curve
/// can fall towards 0 there, and it is the caller that decides whether a collapsed
/// sensor resistance means saturation. NaN (unusable input, or a curve the method
/// cannot evaluate - a = 0 has no solution) makes the caller publish `unknown`.
inline float ppm_from_ratio(float a, float b, float ratio, RegressionMethod method) {
  if (!std::isfinite(ratio) || ratio < 0.0f)
    return NAN;
  if (ratio == 0.0f)
    return ppm_at_zero(a, b, method);

  double log_ppm;
  if (method == RegressionMethod::REGRESSION_METHOD_EXPONENTIAL) {
    log_ppm = std::log10(static_cast<double>(a)) + static_cast<double>(b) * std::log10(static_cast<double>(ratio));
  } else if (method == RegressionMethod::REGRESSION_METHOD_INVERSE) {
    // PPM = (ratio / a)^(1/b).
    log_ppm = (std::log10(static_cast<double>(ratio)) - std::log10(static_cast<double>(a))) / static_cast<double>(b);
  } else if (method == RegressionMethod::REGRESSION_METHOD_LINEAR) {
    log_ppm = (std::log10(static_cast<double>(ratio)) - static_cast<double>(b)) / static_cast<double>(a);
  } else {
    // An unknown method has no formula; evaluating it as one of the known ones
    // would publish a plausible but wrong concentration.
    return NAN;
  }

  if (!std::isfinite(log_ppm))
    return NAN;

  double ppm;
  if (will_overflow(log_ppm)) {
    ppm = log_ppm > 0.0 ? static_cast<double>(FLT_MAX) : 0.0;
  } else {
    ppm = safe_pow(10.0, log_ppm);
  }

  if (std::isnan(ppm))
    return NAN;
  if (std::isinf(ppm))
    ppm = static_cast<double>(FLT_MAX);
  if (ppm < 0.0)
    ppm = 0.0;
  return static_cast<float>(ppm);
}

/// Clamp a PPM reading to the measuring range (NaN stays NaN).
inline float clamp_ppm(float ppm, float min_ppm, float max_ppm) {
  if (!std::isfinite(ppm))
    return NAN;
  if (ppm <= 0.0f)
    return 0.0f;
  if (ppm < min_ppm)
    return 0.0f;
  if (ppm > max_ppm)
    return max_ppm;
  return ppm;
}

}  // namespace mqmath

}  // namespace esphome::mq_gas_sensors
