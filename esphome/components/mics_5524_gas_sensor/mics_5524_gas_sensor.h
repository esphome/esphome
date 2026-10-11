#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "mics_math.h"

namespace esphome::mics_5524_gas_sensor {

/// One MiCS-5524 channel: samples the analog output, converts it with the vendor
/// model and publishes the gas concentration.
///
/// The clean-air reference holds `x_air` in volts, the value the analog output
/// reads in clean air.  It comes from `air_reference:` in the configuration or
/// from a clean-air calibration at boot, so the board's load resistor and its
/// divider ratio do not matter.
class MiCS5524GasSensor : public sensor::Sensor, public PollingComponent {
 public:
  /// `source` is the voltage sampler the analog output is read from.  It must
  /// outlive this component, which code generation guarantees.
  explicit MiCS5524GasSensor(voltage_sampler::VoltageSampler *source) : source_(source) {}

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  // ------------------------------------------------------------------ config
  /// Name of the reported gas (e.g. `'H2'`); a code generation literal kept as a view.
  void set_gas(const char *gas) { this->gas_ = StringRef(gas); }
  void set_threshold(float threshold) { this->threshold_ = threshold; }
  void set_gain(float gain) { this->gain_ = gain; }
  void set_vendor_min_ppm(float ppm) { this->vendor_min_ppm_ = ppm; }
  void set_vendor_max_ppm(float ppm) { this->vendor_max_ppm_ = ppm; }
  void set_min_ppm(float ppm) { this->min_ppm_ = ppm; }
  void set_max_ppm(float ppm) { this->max_ppm_ = ppm; }
  void set_vcc(float vcc) { this->vcc_ = vcc; }
  void set_voltage_multiplier(float multiplier) { this->voltage_multiplier_ = multiplier; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  void set_sample_interval(uint32_t interval) { this->sample_interval_ = interval; }
  void set_warmup_time(uint32_t warmup) { this->warmup_time_ = warmup; }
  void set_calibration(bool enabled, uint32_t delay, uint32_t samples);
  void set_air_reference(float air_reference);

  // ----------------------------------------------------------------- runtime
  /// (Re)start the clean-air calibration (deferred until `warmup_time` ended).
  void request_calibration();
  bool is_calibrating() const { return this->calibrating_; }
  /// A clean-air reference (`x_air`) is available.
  bool has_reference() const { return this->reference_configured_ && this->reference_ > 0.0f; }

  float get_reference() const { return this->reference_; }
  float get_ratio() const { return this->ratio_; }
  float get_sensor_voltage() const { return this->sensor_voltage_; }

 protected:
  /// Averaged, scaled output voltage of the sensor (V); the `samples_` conversions
  /// run back to back, so the call returns without waiting.
  float sample_voltage_();
  /// The model's ratio for the current reading.
  float current_ratio_();
  /// Full conversion; returns NAN when no valid reading exists.
  float read_ppm_();
  /// The vendor curve as configured (threshold, gain, vendor range).
  micsmath::VendorCurve vendor_curve_() const;

  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();
  void log_config_();

  /// Time left of the warm-up window, 0 once the sensor is settled.  Elapsed time
  /// is compared instead of a deadline, so the result survives the 32-bit
  /// `millis()` rollover.
  uint32_t warmup_remaining_() const;

  // ------------------------------------------------------------- configuration
  StringRef gas_{StringRef::from_lit("CUSTOM")};
  float threshold_{0.0f};
  float gain_{1.0f};
  float vendor_min_ppm_{0.0f};
  float vendor_max_ppm_{1000.0f};
  float min_ppm_{0.0f};
  float max_ppm_{1000.0f};
  float vcc_{5.0f};
  float voltage_multiplier_{1.0f};
  uint8_t samples_{4};
  /// Minimum spacing between two clean-air calibration samples (ms); a normal reading
  /// never waits for it.  The ~16 ms main loop tick is the practical floor.
  uint32_t sample_interval_{20};
  uint32_t warmup_time_{0};
  voltage_sampler::VoltageSampler *source_{nullptr};

  // -------------------------------------------------------------- calibration
  bool calibration_enabled_{false};
  uint32_t calibration_delay_{0};
  uint32_t calibration_samples_{10};

  // ------------------------------------------------------------------ runtime
  /// Scheduler id of the deferred calibration start, re-armed and cancelled by
  /// `request_calibration()`.
  static constexpr uint32_t CALIBRATION_START_ID = 0;

  bool calibrating_{false};
  /// A calibration waits for its start time; `calibrating_` takes over from there.
  bool calibration_pending_{false};
  uint32_t calibration_last_sample_{0};
  uint32_t calibration_count_{0};
  uint32_t calibration_attempts_{0};
  float calibration_sum_{0.0f};
  /// Loop time the warm-up window started at, 0 without a `warmup_time:`.
  uint32_t warmup_start_{0};
  bool warmup_notified_{false};
  bool warned_no_reference_{false};
  bool warned_voltage_{false};
  float sensor_voltage_{0.0f};
  float air_value_{0.0f};  ///< x = VCC - V_AO, the quantity the vendor model uses
  float ratio_{0.0f};      ///< x / x_air

  /// Clean-air reference: `x_air` in volts.
  float reference_{0.0f};
  bool reference_configured_{false};
};

}  // namespace esphome::mics_5524_gas_sensor
