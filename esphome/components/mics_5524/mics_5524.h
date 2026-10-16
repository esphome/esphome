#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "mics_math.h"

namespace esphome::mics_5524 {

/// One gas of a MiCS-5524: converts the ratio the sensor measured with the vendor
/// curve of that gas and publishes the concentration.
class MiCS5524GasSensor : public sensor::Sensor {
 public:
  /// `gas` is the name used in the log lines (`'CO'`), the four floats are the
  /// constants of the vendor model, both compiled from `coefficients.py`.
  MiCS5524GasSensor(const char *gas, float threshold, float gain, float min_ppm, float max_ppm)
      : gas_(gas), curve_{threshold, gain, min_ppm, max_ppm} {}

  /// Publish the concentration for `ratio`; NAN publishes `unknown`.
  void publish_ratio(float ratio);

  /// Configured measuring range, logged by the sensor that owns this gas.
  float get_min_ppm() const { return this->curve_.min_ppm; }
  float get_max_ppm() const { return this->curve_.max_ppm; }

 protected:
  /// Gas name for the log lines; a code generation literal, never owned.
  const char *gas_;
  micsmath::VendorCurve curve_;
};

/// The MiCS-5524 breakout: samples its single analog output once per update interval
/// and hands the ratio to every configured gas.
///
/// The clean-air reference holds `x_air` in volts, the value the analog output reads
/// in clean air.  It comes from `air_reference:` in the configuration or from a
/// clean-air calibration at boot, so the board's load resistor and its divider ratio
/// do not matter.  The calibration only accepts the average of samples that agree
/// (see `calibration_is_stable()`), so the sensor has to sit in clean, still air.
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
  void set_vcc(float vcc) { this->vcc_ = vcc; }
  void set_voltage_multiplier(float multiplier) { this->voltage_multiplier_ = multiplier; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  void set_warmup_time(uint32_t warmup) { this->warmup_time_ = warmup; }
  /// Enable the clean-air calibration at boot: it starts `delay` ms from now and
  /// averages `samples` readings spaced `sample_interval` ms apart.
  void set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval);
  void set_air_reference(float air_reference) { this->reference_ = air_reference; }

 protected:
  /// Averaged, scaled output voltage of the sensor (V); the `samples_` conversions run
  /// back to back, so the call returns without waiting.
  float sample_voltage_();
  /// Hand `ratio` to every configured gas (NAN publishes `unknown`).
  void publish_ratio_(float ratio);

  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();
  /// Raise the status warning and schedule the next calibration attempt.
  void schedule_calibration_retry_();
  /// A clean-air reference (`x_air`) is available.
  bool has_reference_() const { return this->reference_ > 0.0f; }

  /// True while the warm-up window of `warmup_time_` is still running; ends the window
  /// for good once it has passed.
  ///
  /// The end has to be latched: subtracting two loop timestamps overflows after about
  /// 49.7 days, which would report the window as running again and silence the readings.
  bool in_warmup_();

  // ------------------------------------------------------------- configuration
  voltage_sampler::VoltageSampler *source_{nullptr};
  float vcc_{5.0f};
  float voltage_multiplier_{1.0f};
  uint8_t samples_{4};
  uint32_t warmup_time_{0};

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

  /// Clean-air reference: `x_air` in volts, 0 while there is none.
  float reference_{0.0f};

  // Sized by code generation, one slot per configured gas; a build that runs no code
  // generation (the C++ unit tests) has no count, so this list and the loops over it
  // compile out there.
#ifdef MICS_5524_GAS_COUNT
  StaticVector<MiCS5524GasSensor *, MICS_5524_GAS_COUNT> gases_;
#endif
};

}  // namespace esphome::mics_5524
