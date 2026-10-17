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

/// Ratio convention used to evaluate the regression curve.
enum RatioMode : uint8_t {
  RATIO_RS_R0 = 0,  ///< RS / R0 - what the published datasheet coefficients expect.
  RATIO_R0_RS = 1,  ///< R0 / RS - MQUnifiedsensor::readSensorR0Rs() convention.
};

/// Upper clamp for `setA()` / `setB()` as in MQUnifiedsensor.h.
static constexpr double MQ_MAX_A = 1e30;
static constexpr double MQ_MAX_B = 100.0;

/// pow() with the fast paths of the reference implementation.
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

/// MQUnifiedsensor::setA() clamping.
inline float clamp_a(float a) {
  if (!std::isfinite(a))
    return 0.0f;
  if (a > MQ_MAX_A)
    return static_cast<float>(MQ_MAX_A);
  if (a < -MQ_MAX_A)
    return static_cast<float>(-MQ_MAX_A);
  return a;
}

/// MQUnifiedsensor::setB() clamping.
inline float clamp_b(float b) {
  if (!std::isfinite(b))
    return 0.0f;
  if (b > MQ_MAX_B)
    return static_cast<float>(MQ_MAX_B);
  if (b < -MQ_MAX_B)
    return static_cast<float>(-MQ_MAX_B);
  return b;
}

/// Sensor resistance in kOhm, MQUnifiedsensor::getRS() / calibrate().
/// Returns 0 for a non-positive / non-finite input voltage instead of +inf.
inline float rs_from_voltage(float voltage, float vcc, float rl) {
  if (!std::isfinite(voltage) || voltage <= 0.0f)
    return 0.0f;
  const double rs =
      (static_cast<double>(vcc) * static_cast<double>(rl) / static_cast<double>(voltage)) - static_cast<double>(rl);
  if (!std::isfinite(rs) || rs < 0.0)
    return 0.0f;
  return static_cast<float>(rs);
}

/// RS / R0 (or R0 / RS) ratio, MQUnifiedsensor::readSensorR0Rs() + correction factor.
inline float ratio_from_rs(float rs, float r0, float correction_factor, RatioMode mode) {
  if (!std::isfinite(rs) || rs <= 0.0f || !std::isfinite(r0) || r0 <= 0.0f)
    return 0.0f;
  double ratio;
  if (mode == RATIO_R0_RS) {
    ratio = static_cast<double>(r0) / static_cast<double>(rs);
  } else {
    ratio = static_cast<double>(rs) / static_cast<double>(r0);
  }
  ratio += static_cast<double>(correction_factor);
  if (!std::isfinite(ratio) || ratio <= 0.0)
    return 0.0f;
  return static_cast<float>(ratio);
}

/// R0 from a clean air reading, MQUnifiedsensor::calibrate().
/// `ratio_in_clean_air` is the RS/R0 value taken from the sensor datasheet.
inline float r0_from_clean_air(float rs_air, float ratio_in_clean_air, float correction_factor) {
  if (!std::isfinite(rs_air) || rs_air <= 0.0f || !std::isfinite(ratio_in_clean_air) || ratio_in_clean_air <= 0.0f)
    return 0.0f;
  double r0 = static_cast<double>(rs_air) / static_cast<double>(ratio_in_clean_air);
  r0 += static_cast<double>(correction_factor);
  if (!std::isfinite(r0) || r0 < 0.0)
    return 0.0f;
  return static_cast<float>(r0);
}

/// ratio (+ a, b) -> PPM, MQUnifiedsensor::readSensorR0Rs().
/// FLT_MAX signals "out of range" so the caller can clamp it: a ratio of
/// exactly 0 is RS at 0 (V_AO at VCC), the output of a saturated sensor, and
/// must never be read as clean air. A NaN stays NaN, so the caller publishes
/// `unknown`.
inline float ppm_from_ratio(float a, float b, float ratio, RegressionMethod method) {
  if (!std::isfinite(ratio))
    return NAN;
  if (a == 0.0f)
    return 0.0f;
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

  double ppm;
  if (will_overflow(log_ppm)) {
    ppm = (std::isnan(log_ppm) || log_ppm > 0.0) ? static_cast<double>(FLT_MAX) : 0.0;
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
///
/// `min_ppm` is the noise floor: a reading below it counts as not detected and
/// reports 0 ppm. 0 ppm itself is never lifted to `min_ppm`, so clean air is
/// published as 0 instead of hiding behind the alarm floor.
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
