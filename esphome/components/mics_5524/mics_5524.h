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
  /// `p0`/`p1` are threshold and gain (dfrobot) or a and b (datasheet), from coefficients.py.
  MiCS5524GasSensor(float p0, float p1, float min_ppm, float max_ppm)
      : p0_(p0), p1_(p1), min_ppm_(min_ppm), max_ppm_(max_ppm) {}

  /// Publish the concentration for `ratio` with the formula of `model`; NAN publishes `unknown`.
  void publish_ratio(float ratio, micsmath::ConversionModel model);

  float get_min_ppm() const { return this->min_ppm_; }
  float get_max_ppm() const { return this->max_ppm_; }

 protected:
  float p0_;
  float p1_;
  float min_ppm_;
  float max_ppm_;
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

  void set_conversion(micsmath::ConversionModel conversion) { this->conversion_ = conversion; }
  void set_vcc(float vcc) { this->vcc_ = vcc; }
  void set_rl(float rl) { this->rl_ = rl; }
  void set_voltage_multiplier(float multiplier) { this->voltage_multiplier_ = multiplier; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  void set_warmup_time(uint32_t warmup) { this->warmup_time_ = warmup; }
  /// Base of the flash key of the stored reference, a hash of the component ID and the front end.
  void set_preference_key(uint32_t key) { this->preference_key_ = key; }
  /// Driven active in setup(); EN of the breakout is active low, so `inverted: true` turns that on.
  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  /// Calibration parameters: start after `delay` ms, average `samples` readings `sample_interval` ms
  /// apart; `persist` keeps the result in flash, `at_boot` runs one at boot.
  void set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval, bool persist, bool at_boot);
  /// Clean-air reference from the configuration, x_air (dfrobot) or R0 (datasheet).
  void set_reference(float reference) { this->reference_ = reference; }
  /// Deferred until the warm-up window ends.
  void request_calibration();
  SUB_SENSOR(voltage)
  SUB_SENSOR(ratio)
  // RS only exists in the datasheet model.
  SUB_SENSOR(rs)

 protected:
  /// Averaged, scaled output voltage (V); the conversions run back to back.
  float sample_voltage_();
  /// Value of the active model that the reference is taken from, x = VCC - V_AO (vendor) or
  /// RS (datasheet), or NAN (and a status warning) when the voltage is unusable.
  float model_value_(float voltage);
  void publish_ratio_(float ratio);
  void publish_diagnostics_(float voltage, float ratio, float rs);

  /// Start a calibration after `delay` ms; every start goes through here.
  void schedule_calibration_(uint32_t delay);
  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();
  void schedule_calibration_retry_();
  void save_reference_();
  bool load_reference_();
  bool is_vendor_model_() const { return this->conversion_ == micsmath::CONVERSION_MODEL_DFROBOT; }
  bool has_reference_() const { return this->reference_ > 0.0f; }

  voltage_sampler::VoltageSampler *source_{nullptr};
  float vcc_{5.0f};
  float rl_{10.0f};
  float voltage_multiplier_{1.0f};
  uint32_t warmup_time_{0};
  GPIOPin *enable_pin_{nullptr};

  uint32_t calibration_delay_{0};
  uint32_t calibration_samples_{0};
  /// Spacing of the clean-air calibration samples (ms); the ~16 ms loop tick is the floor.
  uint32_t sample_interval_{20};

  static constexpr uint32_t CALIBRATION_START_ID = 0;
  /// Floor of the delay between two calibration attempts (ms).
  static constexpr uint32_t CALIBRATION_RETRY_MIN_MS = 10000;

  uint32_t calibration_last_sample_{0};
  uint32_t calibration_count_{0};
  uint32_t calibration_attempts_{0};
  float calibration_sum_{0.0f};
  /// Smallest and largest clean-air sample of the run, see calibration_is_stable().
  float calibration_min_{0.0f};
  float calibration_max_{0.0f};

  /// Clean-air reference, x_air in volts or R0 in kOhm, 0 while there is none.
  float reference_{0.0f};

  uint32_t preference_key_{0};
  ESPPreferenceObject reference_pref_;

  // Sized by code generation; the host unit tests run none, so the list compiles out there.
#ifdef MICS_5524_GAS_COUNT
  StaticVector<MiCS5524GasSensor *, MICS_5524_GAS_COUNT> gases_;
#endif

  micsmath::ConversionModel conversion_{micsmath::CONVERSION_MODEL_DFROBOT};
  uint8_t samples_{4};
  bool persist_{false};
  bool calibrate_at_boot_{false};
  /// Set from setup() until the warm-up timeout fires.
  bool warming_up_{false};
  bool calibrating_{false};
  /// A calibration waits for its start time; `calibrating_` takes over from there.
  bool calibration_pending_{false};
  /// The out-of-range warning has been logged; the next usable reading clears it.
  bool input_fault_{false};
};

}  // namespace esphome::mics_5524
