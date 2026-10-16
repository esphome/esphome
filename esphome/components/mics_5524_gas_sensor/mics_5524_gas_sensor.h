#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

#include "mics_math.h"

namespace esphome::mics_5524_gas_sensor {

/// One gas of a MiCS-5524: converts the ratio its hub measured with the curve and the
/// coefficients of that gas and publishes the concentration.
class MiCS5524GasSensor : public sensor::Sensor {
 public:
  /// `gas` is the name used in the log lines (`'CO'`), `conversion` one of
  /// `micsmath::ConversionModel`, and the floats are the constants of both models,
  /// compiled from `coefficients.py`: the vendor threshold/gain and the datasheet
  /// coefficients `a`/`b`, plus the measuring range this gas is published in.
  MiCS5524GasSensor(const char *gas, uint8_t conversion, float threshold, float gain, float a, float b, float min_ppm,
                    float max_ppm)
      : gas_(gas), conversion_(conversion), curve_{threshold, gain, min_ppm, max_ppm}, a_(a), b_(b) {}

  /// Publish the concentration for `ratio`; NAN publishes `unknown`.
  void publish_ratio(float ratio);

  /// Configured measuring range, logged by the hub that owns this gas.
  float get_min_ppm() const { return this->curve_.min_ppm; }
  float get_max_ppm() const { return this->curve_.max_ppm; }
  /// Gas name of the log lines (a code generation literal, never owned).
  const char *get_gas() const { return this->gas_; }
  /// True while this gas is converted with the vendor model; the RS diagnostics of the
  /// hub only mean something in the other one.
  bool is_vendor_model() const { return this->conversion_ == micsmath::CONVERSION_MODEL_DFROBOT; }

 protected:
  const char *gas_;
  uint8_t conversion_;
  /// Vendor constants, and the measuring range both models are clamped to.
  micsmath::VendorCurve curve_;
  float a_{0.0f};  ///< datasheet model: ppm = a * ratio^b
  float b_{0.0f};
};

/// The MiCS-5524 breakout: samples its single analog output once per update interval
/// and hands the ratio to every configured gas.
///
/// The clean-air reference is one field because only one model is active per sensor:
/// `x_air` in volts (vendor) or `R0` in kOhm (datasheet).  It comes from
/// `air_reference:`/`r0:` in the configuration or from a clean-air calibration at boot,
/// so the board's load resistor and its divider ratio do not matter.  The calibration
/// only accepts the average of samples that agree (see `calibration_is_stable()`), so
/// the sensor has to sit in clean, still air.
class MiCS5524Component : public PollingComponent {
 public:
  /// `source` is the voltage sampler the analog output is read from.  It must outlive
  /// this component, which code generation guarantees.
  explicit MiCS5524Component(voltage_sampler::VoltageSampler *source) : source_(source) {}

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

#ifdef MICS_5524_GAS_COUNT
  /// Add one gas of this sensor; code generation calls it once per configured gas.
  void register_gas_sensor(MiCS5524GasSensor *gas) { this->gases_.push_back(gas); }
#endif

  // ------------------------------------------------------------------ config
  void set_conversion(uint8_t conversion) { this->conversion_ = conversion; }
  void set_vcc(float vcc) { this->vcc_ = vcc; }
  /// Load resistor of the divider (kOhm); the datasheet model needs it.
  void set_rl(float rl) { this->rl_ = rl; }
  void set_voltage_multiplier(float multiplier) { this->voltage_multiplier_ = multiplier; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  void set_warmup_time(uint32_t warmup) { this->warmup_time_ = warmup; }
  /// Optional EN pin of the module, driven high in `setup()`.
  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  /// Enable the clean-air calibration at boot: it starts `delay` ms from now and
  /// averages `samples` readings spaced `sample_interval` ms apart.
  void set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval);
  /// Fixed clean-air reference of the vendor model: `x_air` in volts.
  void set_air_reference(float air_reference);
  /// Fixed clean-air reference of the datasheet model: `R0` in kOhm.
  void set_r0(float r0);
  SUB_SENSOR(voltage)
  SUB_SENSOR(ratio)
  // RS only exists in the datasheet model, so its sensor stays silent in the vendor one.
  SUB_SENSOR(rs)

 protected:
  // ---------------------------------------------------------------- pipeline
  /// Averaged, scaled output voltage of the sensor (V); the `samples_` conversions run
  /// back to back, so the call returns without waiting.
  float sample_voltage_();
  /// Ratio of the active model for `voltage`: `x / x_air` (vendor) or `RS / R0`
  /// (datasheet).  NAN while there is no reference or the voltage is unusable.
  float ratio_for_voltage_(float voltage);
  /// Clean-air reference of the active model for `voltage`: `x = VCC - V_AO` in volts
  /// (vendor) or `RS` in kOhm (datasheet), NAN for a voltage that cannot be used.
  float reference_for_voltage_(float voltage) const;
  /// Hand `ratio` to every configured gas (NAN publishes `unknown`).
  void publish_ratio_(float ratio);
  /// Publish the reading to the diagnostic sensors: the voltage always, the ratio
  /// always (both models have one) and RS for the datasheet model only.
  void publish_diagnostics_(float voltage, float ratio, float rs);

  // ------------------------------------------------------------- calibration
  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();
  /// Raise the status warning and schedule the next calibration attempt.
  void schedule_calibration_retry_();
  /// True while the active model is the vendor one.
  bool is_vendor_model_() const { return this->conversion_ == micsmath::CONVERSION_MODEL_DFROBOT; }
  /// A clean-air reference (`x_air` or `R0`) is available.
  bool has_reference_() const { return this->reference_ > 0.0f; }
  /// True while the warm-up window of `warmup_time_` is still running; ends the window
  /// for good once it has passed.
  ///
  /// The end has to be latched: subtracting two loop timestamps overflows after about
  /// 49.7 days, which would report the window as running again and silence the readings.
  bool in_warmup_();

  // ------------------------------------------------------------- configuration
  voltage_sampler::VoltageSampler *source_{nullptr};
  uint8_t conversion_{micsmath::CONVERSION_MODEL_DFROBOT};
  float vcc_{5.0f};
  float rl_{10.0f};
  float voltage_multiplier_{1.0f};
  uint8_t samples_{4};
  uint32_t warmup_time_{0};
  GPIOPin *enable_pin_{nullptr};

  // -------------------------------------------------------------- calibration
  bool calibration_enabled_{false};
  uint32_t calibration_delay_{0};
  uint32_t calibration_samples_{10};
  /// Minimum spacing between two clean-air calibration samples (ms); a normal reading
  /// never waits for it.  The ~16 ms main loop tick is the practical floor.
  uint32_t sample_interval_{20};

  // ------------------------------------------------------------------ runtime
  /// Scheduler id of the deferred calibration start: it waits for the warm-up window to
  /// end and is re-armed for another attempt when a calibration fails.
  static constexpr uint32_t CALIBRATION_START_ID = 0;
  /// Floor of the delay between two calibration attempts (ms): a failed attempt runs the
  /// whole sample run again, so a shorter delay only repeats the same failure.
  static constexpr uint32_t CALIBRATION_RETRY_MIN_MS = 10000;

  bool calibrating_{false};
  /// A calibration waits for its start time; `calibrating_` takes over from there.
  bool calibration_pending_{false};
  uint32_t calibration_last_sample_{0};
  uint32_t calibration_count_{0};
  uint32_t calibration_attempts_{0};
  float calibration_sum_{0.0f};
  /// Smallest and largest clean-air sample of the run; a wide spread means the sensor
  /// was not settled, see `calibration_is_stable()`.
  float calibration_min_{0.0f};
  float calibration_max_{0.0f};
  /// Loop time the warm-up window started at; only read while `in_warmup_()` holds it
  /// open.
  uint32_t warmup_start_{0};
  /// The warm-up window ended; latched, so `in_warmup_()` never reports it again.
  bool warmup_ended_{false};
  bool warned_no_reference_{false};
  bool warned_voltage_{false};

  /// Clean-air reference: `x_air` in volts (vendor) or `R0` in kOhm (datasheet).
  float reference_{0.0f};

  // Diagnostic sub-sensors are declared by the SUB_SENSOR setters above.

  // Sized by code generation, one slot per configured gas; a build that runs no code
  // generation (the C++ unit tests) has no count, so this list and the loops over it
  // compile out there.
#ifdef MICS_5524_GAS_COUNT
  StaticVector<MiCS5524GasSensor *, MICS_5524_GAS_COUNT> gases_;
#endif
};

}  // namespace esphome::mics_5524_gas_sensor
