#pragma once
//
// Pure math of the MQ sensor PPM model.
//
// Ported one-to-one from MQUnifiedsensor.cpp (MQSensorsLib, MIT / esp-iot-solution
// "MQSensorLIB" component by Carlos Delfino, Miguel A. Califa U., Yersson R.
// Carrillo A. and Ghiordy F. Contreras C.).
//
//     V      = adc * VOLT_RESOLUTION / (2^bits - 1)
//     RS     = (VCC * RL) / V - RL
//     ratio  = R0 / RS            (library) or RS / R0 (datasheet, default here)
//     PPM    = a * ratio^b        (exponential regression, method 1)
//     log10(PPM) = (log10(ratio) - b) / a   (linear regression, method 2)
//     PPM    = (ratio / a)^(1/b)            (inverse regression, method 3,
//                                            MQDataScience inverseYaxb())
//
// On top of that the optional temperature/humidity correction of MQDataScience
// (Correction.cpp) is available: it divides the ratio by a coefficient that
// depends on the relative humidity and the ambient temperature
//
//     correction = a + c * exp(b * T)     with a, b, c interpolated over RH
//     ratio_eff  = ratio / correction
//
// which is algebraically the same as their `(ratio / (a * correction))^(1 / b)`.
//
// This header is intentionally free of ESPHome / ESP-IDF dependencies so the very
// same code can be unit tested on the host (see tests/components/mq_gas_sensors/mq_math_test.cpp).
//

#include <cfloat>
#include <cmath>
#include <cstdint>

namespace esphome::mq_gas_sensors::mqmath {

/// Regression model, mirrors MQUnifiedsensor's `_regressionMethod`.
enum RegressionMethod : uint8_t {
  REGRESSION_EXPONENTIAL = 1,  ///< _PPM = a * ratio^b
  REGRESSION_LINEAR = 2,       ///< log10(_PPM) = (log10(ratio) - b) / a
  REGRESSION_INVERSE = 3,      ///< _PPM = (ratio / a)^(1 / b), MQDataScience inverseYaxb()
};

/// Ratio convention used to evaluate the regression curve.
enum RatioMode : uint8_t {
  RATIO_RS_R0 = 0,  ///< RS / R0 - what the published datasheet coefficients expect.
  RATIO_R0_RS = 1,  ///< R0 / RS - MQUnifiedsensor::readSensorR0Rs() convention.
};

/// Upper clamp for `setA()` / `setB()` as in MQUnifiedsensor.h.
static constexpr double MQ_MAX_A = 1e30;
static constexpr double MQ_MAX_B = 100.0;

/// Largest relative spread `(max - min) / mean` a clean-air calibration accepts.
inline constexpr float MAX_CALIBRATION_SPREAD = 0.25f;

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

/// True when the clean-air samples describe a settled input.
///
/// A calibration turns the mean of the samples into the R0 every later reading is
/// divided by, so samples that disagree by more than `MAX_CALIBRATION_SPREAD` of
/// their mean show that the sensor was not in clean, still air: the average is
/// worthless and must not become the reference. Unusable values are never settled.
inline bool calibration_is_stable(float min_value, float max_value, float mean) {
  if (!std::isfinite(min_value) || !std::isfinite(max_value) || !std::isfinite(mean) || mean <= 0.0f)
    return false;
  const double spread = static_cast<double>(max_value) - static_cast<double>(min_value);
  return spread <= static_cast<double>(MAX_CALIBRATION_SPREAD) * static_cast<double>(mean);
}

/// True when a source state is recent enough to be used.
///
/// `has_state()` stays true after the last publish, so a source that stops
/// updating (a failed I2C device, a dead remote source) would otherwise keep
/// correcting readings with a frozen value. `last_ms` is the loop timestamp of
/// the last finite state; the unsigned subtraction also handles the wrap.
inline bool source_is_fresh(bool seen, uint32_t last_ms, uint32_t now_ms, uint32_t max_age_ms) {
  return seen && (now_ms - last_ms) <= max_age_ms;
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
    // PPM = (ratio / a)^(1/b), MQDataScience inverseYaxb().
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

// ---------------------------------------------------------------------------
// Temperature / humidity correction (MQDataScience, "Correction.cpp")
// ---------------------------------------------------------------------------

/// Domain of the correction model: its coefficients are tabulated at RH 33 %
/// and RH 85 %, the temperature range is the datasheet range of the sensors.
static constexpr float MQ_TC_RH_MIN = 33.0f;
static constexpr float MQ_TC_RH_MAX = 85.0f;
static constexpr float MQ_TC_TEMP_MIN = -10.0f;
static constexpr float MQ_TC_TEMP_MAX = 50.0f;

/// Coefficients of `a + c * exp(b * T)` at RH 33 % and RH 85 % (per sensor type).
struct TcCorrectionCoefficients {
  float a33;
  float b33;
  float c33;
  float a85;
  float b85;
  float c85;
};

/// MQDataScience's `fmap()` - linear interpolation, `x0`/`x1` are never equal here.
inline float linear_interpolate(float x, float x0, float x1, float y0, float y1) {
  if (x1 == x0)
    return y0;
  return static_cast<float>((static_cast<double>(x) - static_cast<double>(x0)) *
                                (static_cast<double>(y1) - static_cast<double>(y0)) /
                                (static_cast<double>(x1) - static_cast<double>(x0)) +
                            static_cast<double>(y0));
}

/// Relative humidity (%) and ambient temperature (degC) -> correction
/// coefficient `a + c * exp(b * T)` (MQDataScience `calculateCorrection()`).
/// The inputs are clamped to the fitted domain; unusable inputs report 1.0
/// ("no correction"), so a missing source never invalidates the measurement.
inline float correction_coefficient(float rh, float temperature, const TcCorrectionCoefficients &coeffs) {
  if (!std::isfinite(rh) || !std::isfinite(temperature))
    return 1.0f;

  float rh_clamped = rh;
  if (rh_clamped < MQ_TC_RH_MIN)
    rh_clamped = MQ_TC_RH_MIN;
  if (rh_clamped > MQ_TC_RH_MAX)
    rh_clamped = MQ_TC_RH_MAX;

  float t_clamped = temperature;
  if (t_clamped < MQ_TC_TEMP_MIN)
    t_clamped = MQ_TC_TEMP_MIN;
  if (t_clamped > MQ_TC_TEMP_MAX)
    t_clamped = MQ_TC_TEMP_MAX;

  const float a = linear_interpolate(rh_clamped, MQ_TC_RH_MIN, MQ_TC_RH_MAX, coeffs.a33, coeffs.a85);
  const float b = linear_interpolate(rh_clamped, MQ_TC_RH_MIN, MQ_TC_RH_MAX, coeffs.b33, coeffs.b85);
  const float c = linear_interpolate(rh_clamped, MQ_TC_RH_MIN, MQ_TC_RH_MAX, coeffs.c33, coeffs.c85);

  const double correction = static_cast<double>(a) +
                            static_cast<double>(c) * std::exp(static_cast<double>(b) * static_cast<double>(t_clamped));
  if (!std::isfinite(correction) || correction <= 0.0)
    return 1.0f;
  return static_cast<float>(correction);
}

/// Apply the correction the MQDataScience way: `ratio_eff = ratio / correction`.
/// A missing (1.0), non-finite or non-positive correction leaves the ratio alone.
inline float apply_correction(float ratio, float correction) {
  if (!std::isfinite(ratio) || !std::isfinite(correction) || correction <= 0.0f)
    return ratio;
  return static_cast<float>(static_cast<double>(ratio) / static_cast<double>(correction));
}

/// Inverse of `apply_correction()`: `ratio_eff = ratio * correction`.
///
/// Under `ratio_mode: r0_rs` the ratio grows as the gas concentration falls, so
/// the MQDataScience correction has to move it the other way to keep the same
/// physical meaning.
inline float apply_correction_inverse(float ratio, float correction) {
  if (!std::isfinite(ratio) || !std::isfinite(correction) || correction <= 0.0f)
    return ratio;
  return static_cast<float>(static_cast<double>(ratio) * static_cast<double>(correction));
}

/// Where the correction comes from (mirrors `CORRECTION_MODES` in `__init__.py`).
enum CorrectionMode : uint8_t {
  CORRECTION_NONE = 0,           ///< no compensation (default)
  CORRECTION_MQDATASCIENCE = 1,  ///< `a + c * exp(b * T)`
};

/// How the corrected value is clipped to the configured range.
enum CorrectionClamp : uint8_t {
  CLAMP_ABSOLUTE = 0,  ///< clip to `max_ppm` - the alarm ceiling never moves
  CLAMP_SCALED = 1,    ///< clip to `max_ppm * correction` (MQDataScience behaviour)
};

/// Clamp a (possibly corrected) PPM value to the configured range.
inline float clamp_ppm_corrected(float ppm, float min_ppm, float max_ppm, float correction, CorrectionClamp mode) {
  float upper = max_ppm;
  if (mode == CLAMP_SCALED)
    upper = static_cast<float>(static_cast<double>(max_ppm) * static_cast<double>(correction));
  if (!std::isfinite(upper) || upper < min_ppm)
    upper = max_ppm;
  return clamp_ppm(ppm, min_ppm, upper);
}

}  // namespace esphome::mq_gas_sensors::mqmath
