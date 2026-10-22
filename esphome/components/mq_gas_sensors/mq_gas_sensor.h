#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
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
  SUB_SENSOR(ratio)
  SUB_SENSOR(rs)
  SUB_SENSOR(voltage)
  SUB_SENSOR(correction)
  SUB_SENSOR(r0)

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
  void set_regression_method(mqmath::RegressionMethod method) { this->regression_method_ = method; }
  void set_ratio_mode(mqmath::RatioMode mode) { this->ratio_mode_ = mode; }
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
  void set_correction_mode(mqmath::CorrectionMode mode) { this->correction_mode_ = mode; }
  void set_correction_clamp(mqmath::CorrectionClamp mode) { this->correction_clamp_ = mode; }
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
  /// Identity of the wiring the persisted R0 belongs to: a value stored for a
  /// different supply, scaling or load resistor must never be read back.
  void set_preference_key(uint32_t key) { this->preference_key_ = key; }
#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  void set_log_sensor(text_sensor::TextSensor *sensor) { this->log_sensor_ = sensor; }
#endif
  /// Log the value that is about to be published at `INFO` on every update.  It
  /// is the "normal mode" line: the detailed chain (V / RS / ratio / correction)
  /// stays at `DEBUG` and only appears with the logger tag back at `DEBUG`.
  void set_log_ppm(bool log_ppm) { this->log_ppm_ = log_ppm; }

  /// Console level of a `log_message_()` call.  The order mirrors the verbosity
  /// of ESP-IDF's `esp_log_level_t`: `LOG_SEVERITY_ERROR` is always printed, `LOG_SEVERITY_DEBUG`
  /// only with `logger: level: DEBUG`.  The message is mirrored to `log_sensor:`
  /// either way, so the measurement chain stays readable from Home Assistant.
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

  /// Minimum interval between two *per-update* (`LOG_SEVERITY_DEBUG`) messages mirrored to
  /// `log_sensor_`.  The sensor may poll at 1 Hz, and a text state per second would
  /// flood the Home Assistant recorder - calibration messages, warnings and errors
  /// are always mirrored immediately (the console log is never throttled).
#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  static constexpr uint32_t LOG_SENSOR_DEBUG_INTERVAL_MS = 30000;
#endif

  // ----------------------------------------------------------------- runtime
  bool has_r0() const { return this->r0_ > 0.0f; }

  /// Start a clean-air calibration now - the `mq_gas_sensors.calibrate_r0` action.
  ///
  /// The samples are taken straight away, except while the warm-up window still
  /// runs: the start then waits for the rest of it instead of sampling a sensor
  /// that has not settled.  A successful run replaces the R0 of this boot and is
  /// stored in flash when `persist:` is on; without a `calibration:` block it uses
  /// its defaults (10 samples, not persisted).
  void start_calibration();

  float get_r0() const { return this->r0_; }
  float get_rs() const { return this->rs_; }
  float get_ratio() const { return this->ratio_; }
  float get_correction() const { return this->correction_; }
  float get_sensor_voltage() const { return this->sensor_voltage_; }

 protected:
  /// Averaged, scaled analog output voltage of the sensor (V).  The `samples_`
  /// conversions are taken back to back, so the call returns without waiting.
  float sample_voltage_();
  /// True when `voltage` is a measurement rather than a missing signal.  The one
  /// place `MIN_VALID_VOLTAGE` is applied, shared by `update()` and calibration.
  bool voltage_usable_(float voltage) const { return std::isfinite(voltage) && voltage > MIN_VALID_VOLTAGE; }
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
  /// Raise the status warning and schedule the next calibration attempt.
  void schedule_calibration_retry_();
  /// True while the warm-up window of `warmup_time_` is still running; ends the
  /// window for good once it has passed.
  ///
  /// The end has to be latched: subtracting two loop timestamps overflows after
  /// about 49.7 days, which would report the window as running again and silence
  /// the readings.
  bool in_warmup_();
  void publish_diagnostics_();
  /// Publish the diagnostics of an update that could not produce a reading: the
  /// voltage is a real measurement, RS and the ratio are unknown.
  void publish_fault_diagnostics_();
  /// Publish the current R0 to `r0_sensor` (no-op when it is not configured).
  /// Only called where R0 is set - it does not change between readings.
  void publish_r0_();
  bool save_r0_();
  bool load_r0_();
  void log_config_();
  /// Log the value published by the next `publish_state()` at `INFO`
  /// (`'<type> <gas>': <ppm> ppm`), the "normal mode" counterpart of the
  /// `LOG_SEVERITY_DEBUG` chain in `update()`.  No-op when `log_ppm:` is off or the
  /// reading is invalid; console only - the chain is what reaches `log_sensor_`.
  void log_reading_(float ppm);
  /// Publish a message to `log_sensor_` (no-op when it is not configured);
  /// `LOG_SEVERITY_DEBUG` messages are rate limited to `LOG_SENSOR_DEBUG_INTERVAL_MS`.
#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  void publish_log_(LogSeverity level, const char *message);
#endif
  /// Log a message on the console and mirror it to `log_sensor_`; it is prefixed
  /// with the sensor type and the gas (`'MQ-8 H2': ...`) so several sensors stay
  /// readable.  Pass the format as `LOG_STR(...)`: on ESP8266 it is read from
  /// flash with `vsnprintf_P()`, like the logger does.
  void log_message_(LogSeverity level, const LogString *format, ...);
  /// Clear the status warning a fault just ended, or restate the warning that is
  /// still current - the status slot holds one warning only.
  void update_status_();
  /// End a ratio fault that a valid reading supersedes.
  void clear_ratio_warning_();

  StringRef type_{StringRef::from_lit("MQ")};
  StringRef gas_{StringRef::from_lit("CUSTOM")};
  voltage_sampler::VoltageSampler *source_{nullptr};
  sensor::Sensor *temperature_source_{nullptr};
  sensor::Sensor *humidity_source_{nullptr};

  float a_{0.0f};
  float b_{0.0f};
  mqmath::RegressionMethod regression_method_{mqmath::REGRESSION_EXPONENTIAL};
  mqmath::RatioMode ratio_mode_{mqmath::RATIO_RS_R0};
  float rl_{10.0f};  ///< load resistor of the module (kOhm)
  float r0_{0.0f};   ///< sensor resistance in clean air (kOhm)
  float vcc_{5.0f};  ///< sensor supply (V)
  float ratio_in_clean_air_{0.0f};
  float voltage_multiplier_{1.0f};
  float min_ppm_{0.0f};
  float max_ppm_{10000.0f};
  float correction_factor_{0.0f};
  mqmath::CorrectionMode correction_mode_{mqmath::CORRECTION_NONE};
  mqmath::CorrectionClamp correction_clamp_{mqmath::CLAMP_ABSOLUTE};
  mqmath::TcCorrectionCoefficients tc_{};
  /// Last state of the linked correction sources, used to detect a source that
  /// has stopped updating while `has_state()` stays true.
  uint32_t temperature_last_ms_{0};
  uint32_t humidity_last_ms_{0};
  bool temperature_seen_{false};
  bool humidity_seen_{false};
  uint8_t samples_{2};
  uint32_t sample_interval_{20};
  uint32_t warmup_time_{0};
  bool r0_configured_{false};
  bool log_ppm_{false};  ///< log the published value at INFO on every update (`log_ppm: true`)

  bool calibration_enabled_{false};
  bool persist_{false};
  uint32_t calibration_delay_{0};
  uint32_t calibration_duration_{0};
  uint32_t calibration_samples_{10};

  /// Scheduler id of the deferred calibration start: it waits for the warm-up
  /// window to end and is re-armed for another attempt when a calibration fails.
  static constexpr uint32_t CALIBRATION_START_ID = 0;
  /// Floor of the delay between two calibration attempts (ms): a failed attempt
  /// runs the whole sample run again, so a shorter delay only repeats the failure.
  static constexpr uint32_t CALIBRATION_RETRY_MIN_MS = 10000;

  bool calibrating_{false};
  bool calibration_pending_{false};
  uint32_t calibration_last_sample_{0};
  uint32_t calibration_start_{0};  ///< when the running calibration started
  uint32_t calibration_count_{0};
  uint32_t calibration_attempts_{0};
  /// Why the samples that were not counted failed, so the final message names the
  /// actual fault instead of pointing at the wiring for every cause.
  uint32_t calibration_rejected_low_{0};
  uint32_t calibration_rejected_high_{0};
  uint32_t calibration_rejected_nonfinite_{0};
  uint32_t calibration_rejected_rs_{0};
  float calibration_sum_{0.0f};
  /// Smallest and largest clean-air sample of the run; a wide spread means the
  /// sensor was not settled, see `calibration_is_stable()`.
  float calibration_min_{0.0f};
  float calibration_max_{0.0f};
  /// Loop time the warm-up window started at; only read while `in_warmup_()`
  /// holds it open.
  uint32_t warmup_start_{0};
  /// The warm-up window ended; latched, so `in_warmup_()` never reports it again.
  bool warmup_ended_{false};
  bool warned_no_r0_{false};
  /// A sampler that returned no finite value in the whole averaging window.
  bool warned_sampler_failure_{false};
  /// Latched faults of the analog input, one per direction: each is warned about
  /// once and re-armed when the wiring reads as expected again.
  bool warned_low_voltage_{false};
  bool warned_high_voltage_{false};
  /// The status warning currently reports a wiring fault, so it is the one to
  /// clear once the input reads as expected (a calibration warning is not ours).
  const LogString *wiring_warning_{nullptr};
  /// The warning the status slot holds for R0 (a failed calibration, an R0 that
  /// could not be stored): it is restated when another warning that took the slot
  /// clears.
  const LogString *r0_warning_{nullptr};
  /// A reading the curve cannot evaluate while the wiring itself is fine.
  bool warned_unusable_ratio_{false};
  /// The temperature/humidity sources cannot provide a usable correction.
  bool correction_degraded_{false};
  bool warned_correction_{false};
  /// The first rejected calibration sample was logged (debug only).
  bool calibration_rejection_logged_{false};
  float sensor_voltage_{0.0f};
  float rs_{0.0f};
  float ratio_{0.0f};
  float correction_{1.0f};

#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  text_sensor::TextSensor *log_sensor_{nullptr};
  uint32_t last_log_sensor_debug_{0};  ///< `App.get_loop_component_start_time()` of the last mirrored DEBUG message
#endif

  /// Wiring hash of the stored R0 (`_preference_key()` in ``sensor.py``), folded
  /// into the preference key by `setup()`.
  uint32_t preference_key_{0};

  ESPPreferenceObject r0_pref_{};
};

}  // namespace esphome::mq_gas_sensors
