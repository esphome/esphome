#pragma once
// Pure math of the two MiCS-5524 conversion models; the per-gas constants live in coefficients.py.
// dfrobot: x = VCC - V_ao, ratio = x / x_air (sampled in clean air), ppm = (threshold - ratio) / gain.
// datasheet: RS = (VCC * RL) / V_ao - RL, ratio = RS / R0 (RS in clean air), ppm = a * ratio^b.
// Every helper returns NaN for input that cannot produce a reading. No ESPHome dependencies, so the
// host tests cover it.

#include <cmath>
#include <cstdint>

namespace esphome::mics_5524::micsmath {

/// Generated through CONVERSION_MODELS in __init__.py.
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

/// Share of VCC at or below which the vendor model reads the output as disconnected: clean air
/// sits a volt or two below VCC, so a reading near 0 V is a wiring fault, not gas-free air.
inline constexpr float VENDOR_DEAD_INPUT_FRACTION = 0.05f;

/// x = VCC - V_ao: 0 at VCC (a saturated sensor, never clean air). NaN at or below
/// VENDOR_DEAD_INPUT_FRACTION of VCC (a disconnected pin) and above VCC, which only a wrong vcc or
/// voltage_multiplier produces.
inline float air_value_from_voltage(float voltage, float vcc) {
  if (!std::isfinite(voltage) || !std::isfinite(vcc) || voltage <= VENDOR_DEAD_INPUT_FRACTION * vcc || voltage > vcc)
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

/// Largest RS the datasheet model reads, as a multiple of RL. The datasheet puts the clean-air
/// resistance of a module below about 1.5 MOhm, 150 x the usual 10k RL, so an output voltage
/// below that of this resistance is a disconnected input.
inline constexpr float MAX_RS_OVER_RL = 200.0f;

/// Smallest V_ao the datasheet model reads: the output of RS = MAX_RS_OVER_RL * RL.
inline float datasheet_dead_input_voltage(float vcc) { return vcc / (MAX_RS_OVER_RL + 1.0f); }

/// RS = (VCC * RL) / V - RL in kOhm: 0 at VCC (a saturated sensor). NaN above VCC, which only a
/// wrong vcc or voltage_multiplier produces, and at or below datasheet_dead_input_voltage().
inline float rs_from_voltage(float voltage, float vcc, float rl) {
  if (!std::isfinite(voltage) || !std::isfinite(vcc) || !std::isfinite(rl) || rl <= 0.0f || voltage > vcc ||
      voltage <= datasheet_dead_input_voltage(vcc))
    return NAN;
  return vcc * rl / voltage - rl;
}

/// ratio = RS / R0 (NaN when either value is unusable); 0 is the saturated end.
inline float ratio_from_rs(float rs, float r0) {
  if (!std::isfinite(rs) || rs < 0.0f || !std::isfinite(r0) || r0 <= 0.0f)
    return NAN;
  return rs / r0;
}

/// ppm = a * ratio^b; NaN for a ratio or coefficient that cannot produce a reading. Ratio 0, the
/// saturated end, gives infinity on a falling curve, which clamp_ppm turns into max_ppm.
inline float ppm_from_power_law(float a, float b, float ratio) {
  if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(ratio) || ratio < 0.0f || a <= 0.0f)
    return NAN;

  const float ppm = a * powf(ratio, b);
  return std::isnan(ppm) ? NAN : ppm;
}

/// Clamp to the configured range: below min_ppm counts as not detected and reads 0, 0 itself
/// never changes, and a reading past a finite max_ppm (infinity included) is max_ppm. NaN stays
/// NaN.
inline float clamp_ppm(float ppm, float min_ppm, float max_ppm) {
  if (std::isnan(ppm))
    return NAN;
  if (ppm <= 0.0f)
    return 0.0f;
  if (std::isfinite(min_ppm) && ppm < min_ppm)
    return 0.0f;
  if (std::isfinite(max_ppm) && ppm > max_ppm)
    return max_ppm;
  return std::isfinite(ppm) ? ppm : NAN;
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
