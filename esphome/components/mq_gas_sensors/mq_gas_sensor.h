#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "mq_math.h"

namespace esphome::mq_gas_sensors {

/// One MQ gas sensor: samples the analog output, converts it into the RS/R0
/// ratio and evaluates the built-in regression curve (`PPM = a * ratio^b`).
/// R0 comes from the configuration or from a clean air calibration at boot.
class MQGasSensor : public sensor::Sensor, public PollingComponent {
  SUB_SENSOR(ratio)
  SUB_SENSOR(rs)
  SUB_SENSOR(voltage)

 public:
  explicit MQGasSensor(voltage_sampler::VoltageSampler *source) : source_(source) {}

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_type(const char *type) { this->type_ = StringRef(type); }
  void set_gas(const char *gas) { this->gas_ = StringRef(gas); }
  void set_a(float a) { this->a_ = mqmath::clamp_a(a); }
  void set_b(float b) { this->b_ = mqmath::clamp_b(b); }
  void set_regression_method(uint8_t method) { this->regression_method_ = method; }
  void set_ratio_mode(uint8_t mode) { this->ratio_mode_ = mode; }
  void set_rl(float rl) { this->rl_ = rl; }
  void set_r0(float r0) {
    this->r0_ = r0;
    this->r0_configured_ = true;
  }
  void set_vcc(float vcc) { this->vcc_ = vcc; }
  void set_ratio_in_clean_air(float ratio) { this->ratio_in_clean_air_ = ratio; }
  void set_voltage_multiplier(float multiplier) { this->voltage_multiplier_ = multiplier; }
  void set_min_ppm(float ppm) { this->min_ppm_ = ppm; }
  void set_max_ppm(float ppm) { this->max_ppm_ = ppm; }
  void set_correction_factor(float factor) { this->correction_factor_ = factor; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  /// Enable the clean-air calibration at boot: it starts `delay` ms from now and
  /// averages `samples` readings spaced `sample_interval` ms apart.
  void set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval);
#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  void set_log_sensor(text_sensor::TextSensor *sensor) { this->log_sensor_ = sensor; }
#endif
  /// Log the value that is about to be published at `INFO` on every update. It
  /// is the "normal mode" line: the detailed chain (V / RS / ratio) stays at
  /// `DEBUG` and only appears with the logger tag back at `DEBUG`.
  void set_log_ppm(bool log_ppm) { this->log_ppm_ = log_ppm; }

  /// Console level of a `log_message_()` call. The order mirrors the verbosity
  /// of ESP-IDF's `esp_log_level_t`: `LOG_SEVERITY_ERROR` is always printed,
  /// `LOG_SEVERITY_DEBUG` only with `logger: level: DEBUG`. The message is
  /// mirrored to `log_sensor:` either way, so the measurement chain stays
  /// readable from Home Assistant.
  enum LogSeverity : uint8_t {
    LOG_SEVERITY_DEBUG = 0,
    LOG_SEVERITY_INFO = 1,
    LOG_SEVERITY_WARN = 2,
    LOG_SEVERITY_ERROR = 3,
  };

  /// Size of the message buffer of `log_message_()` (also the text sensor limit).
  static constexpr size_t LOG_BUFFER_SIZE = 160;

  /// Lowest averaged analog output that is still a measurement rather than a
  /// missing signal (open circuit, absent supply, or a failed conversion).
  static constexpr float MIN_VALID_VOLTAGE = 0.01f;

  /// Minimum interval between two *per-update* (`LOG_SEVERITY_DEBUG`) messages
  /// mirrored to `log_sensor_`. The sensor may poll at 1 Hz, and a text state
  /// per second would flood the Home Assistant recorder - calibration messages,
  /// warnings and errors are always mirrored immediately (the console log is
  /// never throttled).
#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  static constexpr uint32_t LOG_SENSOR_DEBUG_INTERVAL_MS = 30000;
#endif

  bool has_r0() const { return this->r0_ > 0.0f; }
  float get_r0() const { return this->r0_; }
  float get_rs() const { return this->rs_; }
  float get_ratio() const { return this->ratio_; }
  float get_sensor_voltage() const { return this->sensor_voltage_; }

 protected:
  /// Averaged, scaled analog output voltage (V); sampled back to back.
  float sample_voltage_();
  /// True when `voltage` is a measurement rather than a missing signal. The one
  /// place `MIN_VALID_VOLTAGE` is applied, shared by `update()` and calibration.
  bool voltage_usable_(float voltage) const { return std::isfinite(voltage) && voltage > MIN_VALID_VOLTAGE; }
  /// RS in kOhm for the current `sensor_voltage_`.
  float current_rs_() const;
  /// RS/R0 (respecting `ratio_mode_`) for the current `sensor_voltage_`.
  float current_ratio_() const;
  /// Full MQUnifiedsensor pipeline; returns NAN when no valid reading exists.
  float read_ppm_();

  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();
  void publish_diagnostics_();
  /// Publish the diagnostics of an update that could not produce a reading: the
  /// voltage is a real measurement, RS and the ratio are unknown.
  void publish_fault_diagnostics_();
  /// Log the value published by the next `publish_state()` at `INFO`
  /// (`'<type> <gas>': <ppm> ppm`), the "normal mode" counterpart of the
  /// `LOG_SEVERITY_DEBUG` chain in `update()`. No-op when `log_ppm:` is off or
  /// the reading is invalid; console only - the chain is what reaches
  /// `log_sensor_`.
  void log_reading_(float ppm);
  /// Publish a message to `log_sensor_` (no-op when it is not configured);
  /// `LOG_SEVERITY_DEBUG` messages are rate limited to
  /// `LOG_SENSOR_DEBUG_INTERVAL_MS`.
#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  void publish_log_(LogSeverity level, const char *message);
#endif
  /// Log a message on the console and mirror it to `log_sensor_`; it is prefixed
  /// with the sensor type and the gas (`'MQ-8 H2': ...`) so several sensors stay
  /// readable. The format is a `LogString` (`LOG_STR(...)`), so on ESP8266 it
  /// stays in flash rather than costing RAM.
  void log_message_(LogSeverity level, const LogString *format, ...);
  /// Clear the status warning a fault just ended, or restate the R0 warning that is
  /// still current - the status slot holds one warning only.
  void refresh_status_warning_();
  /// End a ratio fault that a valid reading supersedes.
  void clear_ratio_warning_();

  StringRef type_{StringRef::from_lit("MQ")};
  StringRef gas_{StringRef::from_lit("CUSTOM")};
  voltage_sampler::VoltageSampler *source_{nullptr};

  float a_{0.0f};
  float b_{0.0f};
  uint8_t regression_method_{mqmath::REGRESSION_EXPONENTIAL};
  uint8_t ratio_mode_{mqmath::RATIO_RS_R0};
  float rl_{10.0f};  ///< load resistor of the module (kOhm)
  float r0_{0.0f};   ///< sensor resistance in clean air (kOhm)
  float vcc_{5.0f};  ///< sensor supply (V)
  float ratio_in_clean_air_{0.0f};
  float voltage_multiplier_{1.0f};
  float min_ppm_{0.0f};
  float max_ppm_{10000.0f};
  float correction_factor_{0.0f};
  uint8_t samples_{2};
  bool r0_configured_{false};
  bool log_ppm_{false};  ///< log the published value at INFO on every update (`log_ppm: true`)

  bool calibration_enabled_{false};
  uint32_t calibration_delay_{0};
  uint32_t calibration_samples_{10};
  /// Minimum spacing between two clean-air calibration samples (ms); a normal reading
  /// never waits for it.  The ~16 ms main loop tick is the practical floor.
  uint32_t sample_interval_{20};

  /// Scheduler id of the deferred calibration start.
  static constexpr uint32_t CALIBRATION_START_ID = 0;

  bool calibrating_{false};
  bool calibration_pending_{false};
  uint32_t calibration_last_sample_{0};
  uint32_t calibration_count_{0};
  uint32_t calibration_attempts_{0};
  /// Why the samples that were not counted failed, so the final message names the
  /// actual fault instead of pointing at the wiring for every cause.
  uint32_t calibration_rejected_low_{0};
  uint32_t calibration_rejected_high_{0};
  uint32_t calibration_rejected_nonfinite_{0};
  uint32_t calibration_rejected_rs_{0};
  float calibration_sum_{0.0f};
  /// The first rejected calibration sample was logged (debug only).
  bool calibration_rejection_logged_{false};
  bool warned_no_r0_{false};
  /// One flag per fault kind, so each is reported once - and again when the
  /// fault changes.
  bool warned_sampler_failure_{false};
  bool warned_low_voltage_{false};
  bool warned_high_voltage_{false};
  /// A reading the curve cannot evaluate while the wiring itself is fine.
  bool warned_unusable_ratio_{false};
  /// True while a voltage fault is the reason the status warning is up, so a
  /// reading that turns good again clears that warning and no other.
  bool wiring_warning_{false};
  /// The warning the status slot holds for R0 (a failed calibration): it is restated
  /// when another warning that took the slot clears.
  const LogString *r0_warning_{nullptr};
  float sensor_voltage_{0.0f};
  float rs_{0.0f};
  float ratio_{0.0f};

#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  text_sensor::TextSensor *log_sensor_{nullptr};
  uint32_t last_log_sensor_debug_{0};  ///< `App.get_loop_component_start_time()` of the last mirrored DEBUG message
#endif
};

}  // namespace esphome::mq_gas_sensors
