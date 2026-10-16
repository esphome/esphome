#include "mq_gas_sensor.h"

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mq_gas_sensors {

ESPHOME_LOG_TAG(TAG, "mq_gas_sensors");

/// Human readable name of a `mqmath::RegressionMethod` value.
static const char *regression_method_name(uint8_t method) {
  if (method == mqmath::REGRESSION_LINEAR)
    return "linear";
  if (method == mqmath::REGRESSION_INVERSE)
    return "inverse";
  return "exponential";
}

void MQGasSensor::publish_log_(LogSeverity level, const char *message) {
  if (this->log_sensor_ == nullptr)
    return;
  if (level == LOG_SEVERITY_DEBUG) {
    // The per-update line: at a 1 s update interval a text state per second would
    // flood the Home Assistant recorder, so it is mirrored at most every
    // LOG_SENSOR_DEBUG_INTERVAL_MS (the console log keeps every line).
    const uint32_t now = App.get_loop_component_start_time();
    if (this->last_log_sensor_debug_ != 0 && now - this->last_log_sensor_debug_ < LOG_SENSOR_DEBUG_INTERVAL_MS)
      return;
    this->last_log_sensor_debug_ = now;
  }
  this->log_sensor_->publish_state(message);
}

void MQGasSensor::log_message_(LogSeverity level, const char *format, ...) {
  char message[LOG_BUFFER_SIZE];
  const int prefix = std::snprintf(message, sizeof(message), "'%s %s': ", this->type_.c_str(), this->gas_.c_str());
  if (prefix > 0) {
    const size_t used = std::min(static_cast<size_t>(prefix), sizeof(message) - 1);
    va_list args;
    va_start(args, format);
    std::vsnprintf(message + used, sizeof(message) - used, format, args);
    va_end(args);
  }

  switch (level) {
    case LOG_SEVERITY_ERROR:
      ESP_LOGE(TAG, "%s", message);
      break;
    case LOG_SEVERITY_WARN:
      ESP_LOGW(TAG, "%s", message);
      break;
    case LOG_SEVERITY_INFO:
      ESP_LOGI(TAG, "%s", message);
      break;
    default:
      ESP_LOGD(TAG, "%s", message);
      break;
  }

  this->publish_log_(level, message);
}

void MQGasSensor::log_reading_(float ppm) {
  if (!this->log_ppm_ || !std::isfinite(ppm))
    return;
  // Console only: the detailed chain is what reaches `log_sensor_` (rate
  // limited there), and a text state per second would flood the recorder.
  ESP_LOGI(TAG, "'%s %s': %.1f ppm", this->type_.c_str(), this->gas_.c_str(), ppm);
}

void MQGasSensor::setup() {
  if (this->r0_configured_) {
    this->log_message_(LOG_SEVERITY_INFO, "using the R0 from the configuration (%.4f kOhm)", this->r0_);
  }

  if (this->calibration_enabled_) {
    if (this->has_r0()) {
      this->log_message_(LOG_SEVERITY_INFO, "R0 already known (%.4f kOhm) - automatic calibration skipped", this->r0_);
    } else {
      this->calibration_pending_ = true;
      this->set_timeout(CALIBRATION_START_ID, this->calibration_delay_, [this]() { this->begin_calibration_(); });
      this->log_message_(LOG_SEVERITY_INFO, "R0 calibration in %" PRIu32 " s - the sensor must be in clean air",
                         this->calibration_delay_ / 1000);
    }
  } else if (!this->has_r0()) {
    this->log_message_(LOG_SEVERITY_WARN,
                       "no R0 available, set 'r0:' or 'calibration:' - the PPM value stays unknown until then");
  }

  // The loop only samples while a clean-air calibration runs; `update()` publishes.
  this->disable_loop();
}

void MQGasSensor::dump_config() {
  LOG_SENSOR("", "MQ gas sensor", this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Type: %s, target gas: %s", this->type_.c_str(), this->gas_.c_str());
  ESP_LOGCONFIG(TAG, "  Curve: %s (a=%.6g, b=%.6g)", regression_method_name(this->regression_method_), this->a_,
                this->b_);
  ESP_LOGCONFIG(TAG, "  VCC: %.2f V, RL: %.2f kOhm, AO multiplier: x%.3f, range: %.1f - %.1f ppm", this->vcc_,
                this->rl_, this->voltage_multiplier_, this->min_ppm_, this->max_ppm_);
  ESP_LOGCONFIG(TAG, "  RS/R0 in clean air: %.2f", this->ratio_in_clean_air_);
  if (this->has_r0()) {
    ESP_LOGCONFIG(TAG, "  R0: %.4f kOhm (%s)", this->r0_,
                  this->r0_configured_ ? LOG_STR_LITERAL("configured") : LOG_STR_LITERAL("calibrated"));
  } else {
    ESP_LOGCONFIG(TAG, "  R0: not available yet");
  }
  ESP_LOGCONFIG(TAG, "  AO samples per reading: %u, calibration: %s, sample spacing: %" PRIu32 " ms",
                static_cast<unsigned>(this->samples_),
                this->calibration_enabled_ ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"),
                this->sample_interval_);
}

float MQGasSensor::sample_voltage_() {
  // The conversions are taken back to back: the sampler averages internally as
  // configured, and waiting between them would block the main loop.
  float sum = 0.0f;
  for (uint8_t i = 0; i < this->samples_; i++)
    sum += this->source_->sample();
  return (sum / static_cast<float>(this->samples_)) * this->voltage_multiplier_;
}

float MQGasSensor::current_rs_() const { return mqmath::rs_from_voltage(this->sensor_voltage_, this->vcc_, this->rl_); }

float MQGasSensor::current_ratio_() const {
  return mqmath::ratio_from_rs(this->rs_, this->r0_, this->correction_factor_,
                               static_cast<mqmath::RatioMode>(this->ratio_mode_));
}

float MQGasSensor::read_ppm_() {
  const float ppm = mqmath::ppm_from_ratio(this->a_, this->b_, this->ratio_,
                                           static_cast<mqmath::RegressionMethod>(this->regression_method_));
  if (!std::isfinite(ppm))
    return NAN;
  return mqmath::clamp_ppm(ppm, this->min_ppm_, this->max_ppm_);
}

void MQGasSensor::publish_diagnostics_() {
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(this->sensor_voltage_);
  if (this->rs_sensor_ != nullptr)
    this->rs_sensor_->publish_state(this->rs_);
  if (this->ratio_sensor_ != nullptr)
    this->ratio_sensor_->publish_state(this->ratio_);
}

void MQGasSensor::update() {
  if (this->is_failed())
    return;

  if (this->calibrating_) {
    // The R0 is about to change: never keep a value that was computed with the
    // previous one. The message also reaches `log_sensor:`.
    this->publish_state(NAN);
    this->log_message_(LOG_SEVERITY_DEBUG,
                       "update skipped, a calibration is running - the sensor must be in clean air");
    return;
  }

  if (this->calibration_pending_) {
    this->publish_state(NAN);
    return;
  }

  if (!this->has_r0()) {
    if (!this->warned_no_r0_) {
      this->warned_no_r0_ = true;
      this->log_message_(LOG_SEVERITY_WARN, "R0 unknown - set 'r0:' or 'calibration:' in the configuration");
    }
    this->publish_state(NAN);
    return;
  }

  this->sensor_voltage_ = this->sample_voltage_();
  if (!std::isfinite(this->sensor_voltage_) || this->sensor_voltage_ <= 0.01f) {
    if (!this->warned_voltage_) {
      this->warned_voltage_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         "analog output reads %.4f V - open circuit, missing supply or bad "
                         "divider? check the AO wiring (RS would be invalid)",
                         this->sensor_voltage_);
    }
    this->rs_ = 0.0f;
    this->ratio_ = 0.0f;
    this->publish_diagnostics_();
    this->publish_state(NAN);
    return;
  }
  if (this->sensor_voltage_ >= this->vcc_) {
    // Only a wrong `vcc:` / divider reads at or above the supply, and reading it
    // as the saturated end of the range would turn a configuration error into what
    // looks like a real measurement.
    if (!this->warned_voltage_) {
      this->warned_voltage_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         "analog output reads %.4f V, at or above the configured vcc (%.2f V) - check 'vcc:' and the "
                         "divider",
                         this->sensor_voltage_, this->vcc_);
    }
    this->rs_ = 0.0f;
    this->ratio_ = 0.0f;
    this->publish_diagnostics_();
    this->publish_state(NAN);
    return;
  }
  this->warned_voltage_ = false;

  this->rs_ = this->current_rs_();
  this->ratio_ = this->current_ratio_();
  const float ppm = this->read_ppm_();

  this->log_reading_(ppm);
  this->log_message_(LOG_SEVERITY_DEBUG, "V=%.4f V, RS=%.4f kOhm, ratio=%.4f -> %.1f ppm", this->sensor_voltage_,
                     this->rs_, this->ratio_, ppm);

  this->publish_diagnostics_();
  this->publish_state(ppm);
}

void MQGasSensor::set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval) {
  this->calibration_enabled_ = true;
  this->calibration_delay_ = delay;
  this->calibration_samples_ = samples;
  this->sample_interval_ = sample_interval;
}

void MQGasSensor::loop() {
  // The loop runs only while a calibration samples; the deferred start is a timer.
  if (!this->calibrating_)
    return;

  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->calibration_last_sample_ < this->sample_interval_)
    return;

  this->calibration_last_sample_ = now;
  this->process_calibration_();
}

void MQGasSensor::begin_calibration_() {
  this->calibration_pending_ = false;
  this->calibrating_ = true;
  this->calibration_last_sample_ = App.get_loop_component_start_time();
  this->calibration_count_ = 0;
  this->calibration_attempts_ = 0;
  this->calibration_sum_ = 0.0f;
  this->enable_loop();

  this->log_message_(LOG_SEVERITY_INFO,
                     "calibrating R0 (RS/R0 in clean air = %.2f), %" PRIu32 " samples - keep the "
                     "sensor in clean air",
                     this->ratio_in_clean_air_, this->calibration_samples_);
}

void MQGasSensor::process_calibration_() {
  this->calibration_attempts_++;

  const float voltage = this->sample_voltage_();
  if (std::isfinite(voltage) && voltage > 0.01f && voltage < this->vcc_) {
    this->sensor_voltage_ = voltage;
    this->rs_ = this->current_rs_();
    const float r0 = mqmath::r0_from_clean_air(this->rs_, this->ratio_in_clean_air_, this->correction_factor_);
    if (r0 > 0.0f) {
      this->calibration_sum_ += r0;
      this->calibration_count_++;
    }
  }

  if (this->calibration_attempts_ >= this->calibration_samples_)
    this->finish_calibration_();
}

void MQGasSensor::finish_calibration_() {
  this->calibrating_ = false;
  this->disable_loop();

  const uint32_t attempts = this->calibration_attempts_;
  const uint32_t valid = this->calibration_count_;

  if (valid == 0) {
    this->log_message_(LOG_SEVERITY_ERROR,
                       "calibration failed, none of the %" PRIu32 " samples produced a valid RS - check the AO wiring "
                       "and the supply",
                       attempts);
    return;
  }

  this->r0_ = this->calibration_sum_ / static_cast<float>(valid);

  this->log_message_(LOG_SEVERITY_INFO, "R0 = %.4f kOhm (%" PRIu32 "/%" PRIu32 " samples valid)", this->r0_, valid,
                     attempts);
  this->log_message_(LOG_SEVERITY_INFO, "hard-code 'r0: %.4f' in the YAML to skip the calibration at boot", this->r0_);
}

}  // namespace esphome::mq_gas_sensors
