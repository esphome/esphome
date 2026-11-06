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
inline float rs_from_voltage(float voltage, float vcc, float rl) {
  if (!std::isfinite(voltage) || voltage <= 0.0f)
    return NAN;
  const double rs =
      (static_cast<double>(vcc) * static_cast<double>(rl) / static_cast<double>(voltage)) - static_cast<double>(rl);
  if (!std::isfinite(rs) || rs < 0.0)
    return NAN;
  return static_cast<float>(rs);
}

/// RS / R0 (or R0 / RS) ratio, MQUnifiedsensor::readSensorR0Rs() + correction
/// factor, which is added here and nowhere else, so that R0 stays the value a
/// clean air calibration measured.
///
/// A ratio of 0 is the saturated end of the measuring range, which
/// `ppm_from_ratio()` turns into the top of the range. An unusable input gives
/// NaN and the caller publishes `unknown`.
inline float ratio_from_rs(float rs, float r0, float correction_factor, RatioMode mode) {
  if (!std::isfinite(rs) || rs < 0.0f)
    return NAN;
  // RS = 0 is a real reading (V_AO at VCC): the saturated end in either
  // convention, since RS / R0 falls to 0 and R0 / RS grows without bound.
  if (rs == 0.0f)
    return 0.0f;
  if (!std::isfinite(r0) || r0 <= 0.0f)
    return NAN;
  double ratio;
  if (mode == RATIO_R0_RS) {
    ratio = static_cast<double>(r0) / static_cast<double>(rs);
  } else {
    ratio = static_cast<double>(rs) / static_cast<double>(r0);
  }
  ratio += static_cast<double>(correction_factor);
  if (!std::isfinite(ratio))
    return NAN;
  if (ratio <= 0.0) {
    // The correction factor moved the ratio past zero. RS / R0 falls as the
    // concentration rises, so in the datasheet convention this is a reading above
    // the range the curve covers: it reports as the saturated end instead of
    // `unknown`, which would hide it. R0 / RS moves the other way, so the ratio
    // then sits below the clean-air end of the curve: that stays unknown.
    return mode == RATIO_R0_RS ? NAN : 0.0f;
  }
  return static_cast<float>(ratio);
}

/// R0 from a clean air reading, MQUnifiedsensor::calibrate().
/// `ratio_in_clean_air` is the RS/R0 value taken from the sensor datasheet. The
/// correction factor plays no part here: applied to the ratio, it leaves clean
/// air at `ratio_in_clean_air` plus the factor, applied here it would move R0 by
/// a resistance that has nothing to do with the sensor.
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
/// must never be read as clean air. NaN (unusable input, or coefficients the
/// method cannot evaluate - a curve with a = 0 has no solution) makes the caller
/// publish `unknown`.
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
  } else if (method == REGRESSION_LINEAR) {
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

}  // namespace esphome::mq_gas_sensors::mqmath
