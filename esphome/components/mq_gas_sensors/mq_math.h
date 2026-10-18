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

namespace esphome::mq_gas_sensors::mqmath {

/// Regression model, mirrors MQUnifiedsensor's `_regressionMethod`.
enum RegressionMethod : uint8_t {
  REGRESSION_EXPONENTIAL = 1,  ///< _PPM = a * ratio^b
  REGRESSION_LINEAR = 2,       ///< log10(_PPM) = (log10(ratio) - b) / a
  REGRESSION_INVERSE = 3,      ///< _PPM = (ratio / a)^(1 / b)
};

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

/// R0 from a clean air reading, MQUnifiedsensor::calibrate().
/// `ratio_in_clean_air` is the RS/R0 value taken from the sensor datasheet.
inline float r0_from_clean_air(float rs_air, float ratio_in_clean_air) {
  if (!std::isfinite(rs_air) || rs_air <= 0.0f || !std::isfinite(ratio_in_clean_air) || ratio_in_clean_air <= 0.0f)
    return 0.0f;
  const double r0 = static_cast<double>(rs_air) / static_cast<double>(ratio_in_clean_air);
  if (!std::isfinite(r0) || r0 < 0.0)
    return 0.0f;
  return static_cast<float>(r0);
}

/// ratio (+ a, b) -> PPM, MQUnifiedsensor::readSensorR0Rs().
/// FLT_MAX signals "above the range" so the caller can clamp it: a ratio of
/// exactly 0 is RS at 0 (V_AO at VCC), the output of a saturated sensor, and
/// must never be read as clean air. NaN (unusable input, or a curve the method
/// cannot evaluate - a = 0 has no solution) makes the caller publish `unknown`.
inline float ppm_from_ratio(float a, float b, float ratio, RegressionMethod method) {
  if (!std::isfinite(ratio))
    return NAN;
  if (ratio <= 0.0f)
    return FLT_MAX;

  double log_ppm;
  if (method == REGRESSION_EXPONENTIAL) {
    log_ppm = std::log10(static_cast<double>(a)) + static_cast<double>(b) * std::log10(static_cast<double>(ratio));
  } else if (method == REGRESSION_INVERSE) {
    // PPM = (ratio / a)^(1/b).
    log_ppm = (std::log10(static_cast<double>(ratio)) - std::log10(static_cast<double>(a))) / static_cast<double>(b);
  } else {
    log_ppm = (std::log10(static_cast<double>(ratio)) - static_cast<double>(b)) / static_cast<double>(a);
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

}  // namespace esphome::mq_gas_sensors::mqmath
