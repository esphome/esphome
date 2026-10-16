#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include "mics_math.h"

namespace esphome::mics_5524 {

/// One gas of a MiCS-5524: converts the measured ratio with the curve of that gas.
class MiCS5524GasSensor : public sensor::Sensor {
 public:
  /// `gas` is the log name, `conversion` a micsmath::ConversionModel; the floats are the
  /// constants of both models from coefficients.py.
  MiCS5524GasSensor(const char *gas, uint8_t conversion, float threshold, float gain, float a, float b, float min_ppm,
                    float max_ppm)
      : gas_(gas), conversion_(conversion), curve_{threshold, gain, min_ppm, max_ppm}, a_(a), b_(b) {}

  /// Publish the concentration for `ratio`; NAN publishes `unknown`.
  void publish_ratio(float ratio);

  float get_min_ppm() const { return this->curve_.min_ppm; }
  float get_max_ppm() const { return this->curve_.max_ppm; }
  const char *get_gas() const { return this->gas_; }
  /// The RS diagnostics of the hub only mean something in the datasheet model.
  bool is_vendor_model() const { return this->conversion_ == micsmath::CONVERSION_MODEL_DFROBOT; }

 protected:
  /// Code generation literal, never owned.
  const char *gas_;
  uint8_t conversion_;
  /// Vendor constants and the range both models are clamped to.
  micsmath::VendorCurve curve_;
  float a_{0.0f};  ///< datasheet model
  float b_{0.0f};
};

/// The MiCS-5524 breakout: one analog output, converted for every configured gas against the
/// clean-air reference, x_air (vendor) or R0 (datasheet), from the configuration, flash or a
/// calibration at boot.
class MiCS5524Component : public PollingComponent {
 public:
  /// `source` must outlive this component, which code generation guarantees.
  explicit MiCS5524Component(voltage_sampler::VoltageSampler *source) : source_(source) {}

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

#ifdef MICS_5524_GAS_COUNT
  void register_gas_sensor(MiCS5524GasSensor *gas) { this->gases_.push_back(gas); }
#endif

  void set_conversion(uint8_t conversion) { this->conversion_ = conversion; }
  void set_vcc(float vcc) { this->vcc_ = vcc; }
  void set_rl(float rl) { this->rl_ = rl; }
  void set_voltage_multiplier(float multiplier) { this->voltage_multiplier_ = multiplier; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  void set_warmup_time(uint32_t warmup) { this->warmup_time_ = warmup; }
  /// Driven active in setup(); EN of the breakout is active low, so `inverted: true` turns that on.
  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  /// Calibrate at boot: start after `delay` ms, average `samples` readings `sample_interval` ms
  /// apart; `persist` keeps the result in flash across reboots.
  void set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval, bool persist);
  void set_air_reference(float air_reference);
  void set_r0(float r0);
  /// Deferred until the warm-up window ends.
  void request_calibration();
  bool is_calibrating() const { return this->calibrating_; }
  bool has_reference() const { return this->reference_ > 0.0f; }
  float get_reference() const { return this->reference_; }
  SUB_SENSOR(voltage)
  SUB_SENSOR(ratio)
  // RS only exists in the datasheet model.
  SUB_SENSOR(rs)

 protected:
  /// Averaged, scaled output voltage (V); the conversions run back to back.
  float sample_voltage_();
  /// Ratio of the active model; NAN without a reference or for an unusable voltage.
  float ratio_for_voltage_(float voltage);
  /// Clean-air value of the active model: x = VCC - V_AO (vendor) or RS (datasheet).
  float reference_for_voltage_(float voltage) const;
  void publish_ratio_(float ratio);
  /// RS is published for the datasheet model only.
  void publish_diagnostics_(float voltage, float ratio, float rs);

  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();
  void schedule_calibration_retry_();
  void save_reference_();
  bool load_reference_();
  bool is_vendor_model_() const { return this->conversion_ == micsmath::CONVERSION_MODEL_DFROBOT; }
  bool has_reference_() const { return this->reference_ > 0.0f; }

  /// True while the warm-up window is running; latched once it has passed, since the
  /// timestamp subtraction wraps after about 49.7 days.
  bool in_warmup_();

  voltage_sampler::VoltageSampler *source_{nullptr};
  uint8_t conversion_{micsmath::CONVERSION_MODEL_DFROBOT};
  float vcc_{5.0f};
  float rl_{10.0f};
  float voltage_multiplier_{1.0f};
  uint8_t samples_{4};
  uint32_t warmup_time_{0};
  GPIOPin *enable_pin_{nullptr};

  bool calibration_enabled_{false};
  bool persist_{false};
  uint32_t calibration_delay_{0};
  uint32_t calibration_samples_{10};
  /// Spacing of the clean-air calibration samples (ms); the ~16 ms loop tick is the floor.
  uint32_t sample_interval_{20};

  static constexpr uint32_t CALIBRATION_START_ID = 0;
  /// Floor of the delay between two calibration attempts (ms).
  static constexpr uint32_t CALIBRATION_RETRY_MIN_MS = 10000;

  bool calibrating_{false};
  /// A calibration waits for its start time; `calibrating_` takes over from there.
  bool calibration_pending_{false};
  uint32_t calibration_last_sample_{0};
  uint32_t calibration_count_{0};
  uint32_t calibration_attempts_{0};
  float calibration_sum_{0.0f};
  /// Smallest and largest clean-air sample of the run, see calibration_is_stable().
  float calibration_min_{0.0f};
  float calibration_max_{0.0f};
  uint32_t warmup_start_{0};
  /// Latched end of the warm-up window.
  bool warmup_ended_{false};
  bool warned_no_reference_{false};
  bool warned_voltage_{false};

  /// Clean-air reference, x_air in volts or R0 in kOhm, 0 while there is none.
  float reference_{0.0f};

  ESPPreferenceObject reference_pref_;

  // Sized by code generation; the host unit tests run none, so the list compiles out there.
#ifdef MICS_5524_GAS_COUNT
  StaticVector<MiCS5524GasSensor *, MICS_5524_GAS_COUNT> gases_;
#endif
};

}  // namespace esphome::mics_5524
