#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "mics_math.h"

namespace esphome::mics_5524_gas_sensor {

/// One MiCS-5524 channel: samples the analog output, converts it with the
/// configured model and publishes the gas concentration.
///
/// The clean-air reference is one field because only one model is active per
/// sensor: `x_air` in volts (vendor) or `R0` in kOhm (datasheet).  The preference
/// key carries the conversion model, so a stored value of the other model is never
/// used.  It comes from the configuration (`air_reference:` / `r0:`) or from the
/// clean-air calibration at boot, so the board's load resistor and its divider
/// ratio do not matter.
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
  void set_conversion(uint8_t conversion) { this->conversion_ = conversion; }
  void set_threshold(float threshold) { this->threshold_ = threshold; }
  void set_gain(float gain) { this->gain_ = gain; }
  void set_vendor_min_ppm(float ppm) { this->vendor_min_ppm_ = ppm; }
  void set_vendor_max_ppm(float ppm) { this->vendor_max_ppm_ = ppm; }
  void set_a(float a) { this->a_ = a; }
  void set_b(float b) { this->b_ = b; }
  void set_min_ppm(float ppm) { this->min_ppm_ = ppm; }
  void set_max_ppm(float ppm) { this->max_ppm_ = ppm; }
  void set_rl(float rl) { this->rl_ = rl; }
  void set_vcc(float vcc) { this->vcc_ = vcc; }
  void set_voltage_multiplier(float multiplier) { this->voltage_multiplier_ = multiplier; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  void set_sample_interval(uint32_t interval) { this->sample_interval_ = interval; }
  void set_warmup_time(uint32_t warmup) { this->warmup_time_ = warmup; }
  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  void set_calibration(bool enabled, uint32_t delay, uint32_t samples, bool persist);
  void set_r0(float r0);
  void set_air_reference(float air_reference);
  void set_ratio_sensor(sensor::Sensor *sensor) { this->ratio_sensor_ = sensor; }
  void set_rs_sensor(sensor::Sensor *sensor) { this->rs_sensor_ = sensor; }
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  void set_log_sensor(text_sensor::TextSensor *sensor) { this->log_sensor_ = sensor; }
  /// Log the published value at `INFO` on every update (the detailed `DEBUG` chain
  /// needs the logger tag at `DEBUG`).
  void set_log_ppm(bool log_ppm) { this->log_ppm_ = log_ppm; }

  /// Console level of a `log_message_()` call, ordered like ESP-IDF's
  /// `esp_log_level_t`: ERROR is always printed, DEBUG only with `logger: level: DEBUG`.
  ///
  /// The enum name prefixes the values because library headers ESPHome compiles in
  /// (`dudanov/MideaUART`, NimBLE-Arduino) `#define` the bare `LOG_LEVEL_*` names.
  enum LogSeverity : uint8_t {
    LOG_SEVERITY_DEBUG = 0,
    LOG_SEVERITY_INFO = 1,
    LOG_SEVERITY_WARN = 2,
    LOG_SEVERITY_ERROR = 3,
  };

  /// Size of the message buffer of `log_message_()` (also the text sensor limit).
  static constexpr size_t LOG_BUFFER_SIZE = 160;

  /// Minimum interval between two `LOG_SEVERITY_DEBUG` messages mirrored to
  /// `log_sensor_`: a text state per second would flood the Home Assistant recorder.
  /// Calibration messages, warnings and errors are mirrored immediately.
  static constexpr uint32_t LOG_SENSOR_DEBUG_INTERVAL_MS = 30000;

  // ----------------------------------------------------------------- runtime
  /// (Re)start the clean-air calibration (deferred until `warmup_time` ended).
  void request_calibration();
  bool is_calibrating() const { return this->calibrating_; }
  /// A clean-air reference (x_air or R0) is available.
  bool has_reference() const { return this->reference_configured_ && this->reference_ > 0.0f; }

  float get_reference() const { return this->reference_; }
  float get_rs() const { return this->rs_; }
  float get_ratio() const { return this->ratio_; }
  float get_sensor_voltage() const { return this->sensor_voltage_; }

 protected:
  bool is_vendor_model_() const { return this->conversion_ == micsmath::CONVERSION_MODEL_DFROBOT; }
  /// Averaged, scaled output voltage of the sensor (V); the `samples_` conversions
  /// run back to back, so the call returns without waiting.
  float sample_voltage_();
  /// Sensor resistance (kOhm) for the datasheet model.
  float current_rs_() const;
  /// The model's ratio for the current reading.
  float current_ratio_();
  /// Full conversion; returns NAN when no valid reading exists.
  float read_ppm_();
  /// The vendor curve as configured (threshold, gain, vendor range).
  micsmath::VendorCurve vendor_curve_() const;

  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();
  void publish_diagnostics_();
  void save_reference_();
  bool load_reference_();
  void log_config_();
  /// Log the next published value at `INFO` (`'<gas>': <ppm> ppm`, the "normal mode"
  /// counterpart of the `DEBUG` chain).  Console only; no-op when off or invalid.
  void log_reading_(float ppm);
  /// Publish a message to `log_sensor_` (no-op when it is not configured); the
  /// `LOG_SEVERITY_DEBUG` messages are rate limited to `LOG_SENSOR_DEBUG_INTERVAL_MS`.
  void publish_log_(LogSeverity level, const char *message);
  /// Log on the console and mirror the message to `log_sensor_`, prefixed with the
  /// gas (`'H2': ...`) so several channels stay readable.
  void log_message_(LogSeverity level, const char *format, ...);

  /// Time left of the warm-up window, 0 once the sensor is settled.  Elapsed time
  /// is compared instead of a deadline, so the result survives the 32-bit
  /// `millis()` rollover.
  uint32_t warmup_remaining_() const;

  // ------------------------------------------------------------- configuration
  StringRef gas_{StringRef::from_lit("CUSTOM")};
  uint8_t conversion_{micsmath::CONVERSION_MODEL_DFROBOT};
  float threshold_{0.0f};
  float gain_{1.0f};
  float vendor_min_ppm_{0.0f};
  float vendor_max_ppm_{1000.0f};
  float a_{0.0f};
  float b_{0.0f};
  float min_ppm_{0.0f};
  float max_ppm_{1000.0f};
  float rl_{10.0f};
  float vcc_{5.0f};
  float voltage_multiplier_{1.0f};
  uint8_t samples_{4};
  /// Minimum spacing between two clean-air calibration samples (ms); a normal reading
  /// never waits for it.  The ~16 ms main loop tick is the practical floor.
  uint32_t sample_interval_{20};
  uint32_t warmup_time_{0};
  bool log_ppm_{false};  ///< log the published value at INFO on every update (`log_ppm: true`)
  voltage_sampler::VoltageSampler *source_{nullptr};
  GPIOPin *enable_pin_{nullptr};

  // -------------------------------------------------------------- calibration
  bool calibration_enabled_{false};
  bool persist_{false};
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
  float air_value_{0.0f};  ///< x = VCC - V_AO (vendor model)
  float rs_{0.0f};         ///< RS in kOhm (datasheet model)
  float ratio_{0.0f};      ///< x/x_air (vendor) or RS/R0 (datasheet)

  /// Clean-air reference: x_air in volts (vendor) or R0 in kOhm (datasheet).
  float reference_{0.0f};
  bool reference_configured_{false};

  sensor::Sensor *ratio_sensor_{nullptr};
  sensor::Sensor *rs_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  text_sensor::TextSensor *log_sensor_{nullptr};
  uint32_t last_log_sensor_debug_{0};  ///< `App.get_loop_component_start_time()` of the last mirrored DEBUG message

  ESPPreferenceObject reference_pref_{};
};

}  // namespace esphome::mics_5524_gas_sensor
