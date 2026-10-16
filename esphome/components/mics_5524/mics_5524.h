#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "mics_math.h"

namespace esphome::mics_5524 {

/// One gas of a MiCS-5524: converts the measured ratio with the vendor curve of that gas.
class MiCS5524GasSensor : public sensor::Sensor {
 public:
  /// `gas` is the log name; the floats are the vendor constants from coefficients.py.
  MiCS5524GasSensor(const char *gas, float threshold, float gain, float min_ppm, float max_ppm)
      : gas_(gas), curve_{threshold, gain, min_ppm, max_ppm} {}

  /// Publish the concentration for `ratio`; NAN publishes `unknown`.
  void publish_ratio(float ratio);

  float get_min_ppm() const { return this->curve_.min_ppm; }
  float get_max_ppm() const { return this->curve_.max_ppm; }

 protected:
  /// Code generation literal, never owned.
  const char *gas_;
  micsmath::VendorCurve curve_;
};

/// The MiCS-5524 breakout: one analog output, converted for every configured gas against
/// the clean-air reference x_air from `air_reference:` or a calibration at boot.
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

  void set_vcc(float vcc) { this->vcc_ = vcc; }
  void set_voltage_multiplier(float multiplier) { this->voltage_multiplier_ = multiplier; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  void set_warmup_time(uint32_t warmup) { this->warmup_time_ = warmup; }
  /// Calibrate at boot: start after `delay` ms, average `samples` readings `sample_interval` ms apart.
  void set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval);
  void set_air_reference(float air_reference) { this->reference_ = air_reference; }

 protected:
  /// Averaged, scaled output voltage (V); the conversions run back to back.
  float sample_voltage_();
  void publish_ratio_(float ratio);

  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();
  void schedule_calibration_retry_();
  bool has_reference_() const { return this->reference_ > 0.0f; }

  /// True while the warm-up window is running; latched once it has passed, since the
  /// timestamp subtraction wraps after about 49.7 days.
  bool in_warmup_();

  voltage_sampler::VoltageSampler *source_{nullptr};
  float vcc_{5.0f};
  float voltage_multiplier_{1.0f};
  uint8_t samples_{4};
  uint32_t warmup_time_{0};

  bool calibration_enabled_{false};
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

  /// Clean-air reference x_air in volts, 0 while there is none.
  float reference_{0.0f};

  // Sized by code generation; the host unit tests run none, so the list compiles out there.
#ifdef MICS_5524_GAS_COUNT
  StaticVector<MiCS5524GasSensor *, MICS_5524_GAS_COUNT> gases_;
#endif
};

}  // namespace esphome::mics_5524
