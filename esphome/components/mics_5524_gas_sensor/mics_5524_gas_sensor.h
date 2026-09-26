#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "mics_math.h"

namespace esphome::mics_5524_gas_sensor {

/// One MiCS-5524 channel: samples the analog output, converts it with the
/// configured model and publishes the gas concentration.
///
/// The clean-air calibration value is stored in a single field because only one
/// model is active per sensor: it holds `x_air` in volts for the vendor model
/// (`x = VCC - V_AO`) and `R0` in kOhm for the datasheet model. The flash
/// preference key is versioned with the conversion model, so switching models
/// cannot resurrect a value that belongs to the other one.
class MiCS5524GasSensor : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  // ------------------------------------------------------------------ config
  void set_gas(const std::string &gas) { this->gas_ = gas; }
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
  void set_source(voltage_sampler::VoltageSampler *source) { this->source_ = source; }
  void set_calibration(bool enabled, uint32_t delay, uint32_t samples, bool persist);
  void set_r0(float r0);
  void set_air_reference(float air_reference);
  void set_ratio_sensor(sensor::Sensor *sensor) { this->ratio_sensor_ = sensor; }
  void set_rs_sensor(sensor::Sensor *sensor) { this->rs_sensor_ = sensor; }
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  void set_log_sensor(text_sensor::TextSensor *sensor) { this->log_sensor_ = sensor; }
  /// Log the value that is about to be published at `INFO` on every update.  It
  /// is the "normal mode" line: the detailed chain (V_AO / x or RS / ratio)
  /// stays at `DEBUG` and only appears with the logger tag back at `DEBUG`.
  void set_log_ppm(bool log_ppm) { this->log_ppm_ = log_ppm; }

  /// Console level of a `log_message_()` call.  The order mirrors the verbosity
  /// of ESP-IDF's `esp_log_level_t`: `LOG_LEVEL_ERROR` is always printed, `LOG_LEVEL_DEBUG`
  /// only with `logger: level: DEBUG`.  The message is mirrored to `log_sensor:`
  /// either way, so the measurement chain stays readable from Home Assistant.
  enum LogLevel : uint8_t {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
  };

  /// Size of the message buffer of `log_message_()` (also the text sensor limit).
  static constexpr size_t LOG_BUFFER_SIZE = 160;

  /// Minimum interval between two *per-update* (`LOG_LEVEL_DEBUG`) messages mirrored to
  /// `log_sensor_`.  The sensor may poll at 1 Hz, and a text state per second would
  /// flood the Home Assistant recorder - calibration messages, warnings and errors
  /// are always mirrored immediately (the console log is never throttled).
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
  /// Averaged, scaled analog output voltage of the sensor (V).
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
  /// Log the value published by the next `publish_state()` at `INFO`
  /// (`'<gas>': <ppm> ppm`), the "normal mode" counterpart of the `LOG_LEVEL_DEBUG`
  /// chain in `update()`.  No-op when `log_ppm:` is off or the reading is
  /// invalid; console only - the chain is what reaches `log_sensor_`.
  void log_reading_(float ppm);
  /// Publish a message to `log_sensor_` (no-op when it is not configured);
  /// `LOG_LEVEL_DEBUG` messages are rate limited to `LOG_SENSOR_DEBUG_INTERVAL_MS`.
  void publish_log_(LogLevel level, const char *message);
  /// Log a message on the console and mirror it to `log_sensor_`; it is prefixed
  /// with the gas (`'H2': ...`) so several channels stay readable.
  void log_message_(LogLevel level, const char *format, ...);

  // ------------------------------------------------------------- configuration
  std::string gas_{"CUSTOM"};
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
  bool calibrating_{false};
  bool calibration_pending_{false};
  uint32_t calibration_due_{0};
  uint32_t calibration_start_{0};
  uint32_t calibration_last_sample_{0};
  uint32_t calibration_count_{0};
  uint32_t calibration_attempts_{0};
  float calibration_sum_{0.0f};
  uint32_t warmup_end_{0};
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
  uint32_t last_log_sensor_debug_{0};  ///< `millis()` of the last mirrored DEBUG message

  ESPPreferenceObject reference_pref_{};
};

}  // namespace esphome::mics_5524_gas_sensor
