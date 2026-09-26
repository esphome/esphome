#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "mq_math.h"

namespace esphome::mq_gas_sensors {

/// One MQ gas sensor (MQ-2 ... MQ-309A).
///
/// Port of the MQUnifiedsensor PPM pipeline: the analog output voltage is
/// sampled (averaged), converted into the sensor resistance RS, turned into the
/// RS/R0 ratio and evaluated with the configured regression curve
/// (`PPM = a * ratio^b`). R0 comes from the configuration, from flash or from a
/// clean air calibration.
class MQGasSensor : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  // ------------------------------------------------------------------ config
  void set_type(const char *type) { this->type_ = StringRef(type); }
  void set_gas(const char *gas) { this->gas_ = StringRef(gas); }
  void set_source(voltage_sampler::VoltageSampler *source) { this->source_ = source; }
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
  void set_correction_mode(uint8_t mode) { this->correction_mode_ = mode; }
  void set_correction_clamp(uint8_t mode) { this->correction_clamp_ = mode; }
  void set_tc_coefficients(float a33, float b33, float c33, float a85, float b85, float c85) {
    this->tc_ = mqmath::TcCorrectionCoefficients{a33, b33, c33, a85, b85, c85};
  }
  void set_temperature_source(sensor::Sensor *sensor) { this->temperature_source_ = sensor; }
  void set_humidity_source(sensor::Sensor *sensor) { this->humidity_source_ = sensor; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  void set_sample_interval(uint32_t interval) { this->sample_interval_ = interval; }
  void set_warmup_time(uint32_t warmup) { this->warmup_time_ = warmup; }
  void set_calibration(bool enabled, float ratio_in_clean_air, uint32_t delay, uint32_t duration, uint32_t samples,
                       bool persist);
  void set_ratio_sensor(sensor::Sensor *sensor) { this->ratio_sensor_ = sensor; }
  void set_rs_sensor(sensor::Sensor *sensor) { this->rs_sensor_ = sensor; }
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  void set_correction_sensor(sensor::Sensor *sensor) { this->correction_sensor_ = sensor; }
  void set_log_sensor(text_sensor::TextSensor *sensor) { this->log_sensor_ = sensor; }
  /// Log the value that is about to be published at `INFO` on every update.  It
  /// is the "normal mode" line: the detailed chain (V / RS / ratio / correction)
  /// stays at `DEBUG` and only appears with the logger tag back at `DEBUG`.
  void set_log_ppm(bool log_ppm) { this->log_ppm_ = log_ppm; }

  /// Console level of a `log_message_()` call.  The order mirrors the verbosity
  /// of ESP-IDF's `esp_log_level_t`: `LOG_ERROR` is always printed, `LOG_DEBUG`
  /// only with `logger: level: DEBUG`.  The message is mirrored to `log_sensor:`
  /// either way, so the measurement chain stays readable from Home Assistant.
  enum LogLevel : uint8_t {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
  };

  /// Size of the message buffer of `log_message_()` (also the text sensor limit).
  static constexpr size_t LOG_BUFFER_SIZE = 160;

  /// Minimum interval between two *per-update* (`LOG_DEBUG`) messages mirrored to
  /// `log_sensor_`.  The sensor may poll at 1 Hz, and a text state per second would
  /// flood the Home Assistant recorder - calibration messages, warnings and errors
  /// are always mirrored immediately (the console log is never throttled).
  static constexpr uint32_t LOG_SENSOR_DEBUG_INTERVAL_MS = 30000;

  // ----------------------------------------------------------------- runtime
  /// (Re)start an R0 calibration in clean air (deferred until `warmup_time` ended).
  void request_calibration();
  bool is_calibrating() const { return this->calibrating_; }
  bool has_r0() const { return this->r0_ > 0.0f; }

  float get_r0() const { return this->r0_; }
  float get_rs() const { return this->rs_; }
  float get_ratio() const { return this->ratio_; }
  float get_correction() const { return this->correction_; }
  float get_sensor_voltage() const { return this->sensor_voltage_; }

 protected:
  /// Averaged, scaled analog output voltage of the sensor (V).
  float sample_voltage_();
  /// RS in kOhm for the current `sensor_voltage_`.
  float current_rs_() const;
  /// RS/R0 (respecting `ratio_mode_`) for the current `sensor_voltage_`.
  float current_ratio_() const;
  /// Temperature/humidity correction factor (1.0 = uncorrected).
  float compute_correction_();
  /// Full MQUnifiedsensor pipeline; returns NAN when no valid reading exists.
  float read_ppm_();

  void begin_calibration_();
  void process_calibration_();
  void finish_calibration_();
  void publish_diagnostics_();
  void save_r0_();
  bool load_r0_();
  void log_config_();
  /// Log the value published by the next `publish_state()` at `INFO`
  /// (`'<type> <gas>': <ppm> ppm`), the "normal mode" counterpart of the
  /// `LOG_DEBUG` chain in `update()`.  No-op when `log_ppm:` is off or the
  /// reading is invalid; console only - the chain is what reaches `log_sensor_`.
  void log_reading_(float ppm);
  /// Publish a message to `log_sensor_` (no-op when it is not configured);
  /// `LOG_DEBUG` messages are rate limited to `LOG_SENSOR_DEBUG_INTERVAL_MS`.
  void publish_log_(LogLevel level, const char *message);
  /// Log a message on the console and mirror it to `log_sensor_`; it is prefixed
  /// with the sensor type and the gas (`'MQ-8 H2': ...`) so several sensors stay
  /// readable.
  void log_message_(LogLevel level, const char *format, ...);

  // ------------------------------------------------------------- configuration
  StringRef type_{"MQ"};
  StringRef gas_{"CUSTOM"};
  voltage_sampler::VoltageSampler *source_{nullptr};
  sensor::Sensor *temperature_source_{nullptr};
  sensor::Sensor *humidity_source_{nullptr};

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
  uint8_t correction_mode_{mqmath::CORRECTION_NONE};
  uint8_t correction_clamp_{mqmath::CLAMP_ABSOLUTE};
  mqmath::TcCorrectionCoefficients tc_{};
  uint8_t samples_{2};
  uint32_t sample_interval_{20};
  uint32_t warmup_time_{0};
  bool r0_configured_{false};
  bool log_ppm_{false};  ///< log the published value at INFO on every update (`log_ppm: true`)

  // -------------------------------------------------------------- calibration
  bool calibration_enabled_{false};
  bool persist_{false};
  uint32_t calibration_delay_{0};
  uint32_t calibration_duration_{0};
  uint32_t calibration_samples_{10};

  // ------------------------------------------------------------------ runtime
  bool calibrating_{false};
  bool calibration_pending_{false};
  uint32_t calibration_due_{0};    ///< when the pending calibration starts
  uint32_t calibration_start_{0};  ///< when the running calibration started
  uint32_t calibration_last_sample_{0};
  uint32_t calibration_count_{0};
  uint32_t calibration_attempts_{0};
  float calibration_sum_{0.0f};
  uint32_t warmup_end_{0};
  bool warmup_notified_{false};
  bool warned_no_r0_{false};
  bool warned_voltage_{false};
  bool warned_correction_{false};
  float sensor_voltage_{0.0f};
  float rs_{0.0f};
  float ratio_{0.0f};
  float correction_{1.0f};

  sensor::Sensor *ratio_sensor_{nullptr};
  sensor::Sensor *rs_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *correction_sensor_{nullptr};
  text_sensor::TextSensor *log_sensor_{nullptr};
  uint32_t last_log_sensor_debug_{0};  ///< loop tick of the last mirrored DEBUG message

  ESPPreferenceObject r0_pref_{};
};

}  // namespace esphome::mq_gas_sensors
