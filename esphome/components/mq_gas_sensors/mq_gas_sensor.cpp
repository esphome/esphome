#include "mq_gas_sensor.h"

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::mq_gas_sensors {

static const char *const TAG = "mq_gas_sensors";

/// Version tag mixed into the preference key of the persisted R0 value.
static constexpr uint32_t R0_PREFERENCE_VERSION = 0x00000001;

/// Human readable name of a `mqmath::RegressionMethod` value.
static const char *regression_method_name(uint8_t method) {
  switch (method) {
    case mqmath::REGRESSION_LINEAR:
      return "linear";
    case mqmath::REGRESSION_INVERSE:
      return "inverse";
    default:
      return "exponential";
  }
}

void MQGasSensor::publish_log_(LogLevel level, const char *message) {
  if (this->log_sensor_ == nullptr)
    return;
  if (level == LOG_DEBUG) {
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

void MQGasSensor::log_message_(LogLevel level, const char *format, ...) {
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
    case LOG_ERROR:
      ESP_LOGE(TAG, "%s", message);
      break;
    case LOG_WARN:
      ESP_LOGW(TAG, "%s", message);
      break;
    case LOG_INFO:
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
  this->r0_pref_ = this->make_entity_preference<float>(R0_PREFERENCE_VERSION);

  if (this->source_ == nullptr) {
    this->log_message_(LOG_ERROR, "no voltage source configured");
    this->mark_failed();
    return;
  }

  if (this->r0_configured_) {
    this->log_message_(LOG_INFO, "using the R0 from the configuration (%.4f kOhm)", this->r0_);
  } else if (this->persist_ && this->load_r0_()) {
    this->log_message_(LOG_INFO, "restored R0 from flash (%.4f kOhm)", this->r0_);
  }

  if (this->warmup_time_ > 0) {
    this->warmup_end_ = millis() + this->warmup_time_;
    this->log_message_(LOG_INFO, "warm-up/burn-in of %" PRIu32 " s - no readings published before that",
                       this->warmup_time_ / 1000);
  }

  if (this->calibration_enabled_) {
    if (this->has_r0()) {
      this->log_message_(LOG_INFO,
                         "R0 already known (%.4f kOhm) - automatic calibration skipped, call "
                         "request_calibration() (or clear the stored R0) to recalibrate",
                         this->r0_);
    } else {
      const uint32_t delay = std::max(this->calibration_delay_, this->warmup_time_);
      this->calibration_due_ = millis() + delay;
      this->calibration_pending_ = true;
      this->log_message_(LOG_INFO, "R0 calibration scheduled in %" PRIu32 " s - the sensor must be in clean air",
                         delay / 1000);
    }
  } else if (!this->has_r0()) {
    this->log_message_(LOG_WARN, "no R0 available, set 'r0:' or 'calibration:' - the PPM value stays "
                                 "unknown until then");
  }
}

void MQGasSensor::log_config_() {
  ESP_LOGCONFIG(TAG, "  Type: %s", this->type_.c_str());
  ESP_LOGCONFIG(TAG, "  Target gas: %s", this->gas_.c_str());
  ESP_LOGCONFIG(TAG, "  Curve: %s (a=%.6g, b=%.6g), ratio: %s", regression_method_name(this->regression_method_),
                this->a_, this->b_,
                this->ratio_mode_ == mqmath::RATIO_R0_RS ? LOG_STR_LITERAL("R0/RS (MQUnifiedsensor)")
                                                         : LOG_STR_LITERAL("RS/R0 (datasheet)"));
  ESP_LOGCONFIG(TAG, "  VCC: %.2f V, RL: %.2f kOhm, AO multiplier: x%.3f", this->vcc_, this->rl_,
                this->voltage_multiplier_);
  ESP_LOGCONFIG(TAG, "  Range: %.1f - %.1f ppm, samples: %u x %" PRIu32 " ms", this->min_ppm_, this->max_ppm_,
                static_cast<unsigned>(this->samples_), this->sample_interval_);
  if (this->has_r0()) {
    ESP_LOGCONFIG(TAG, "  R0: %.4f kOhm (%s)", this->r0_,
                  this->r0_configured_ ? LOG_STR_LITERAL("configured") : LOG_STR_LITERAL("calibrated/restored"));
  } else {
    ESP_LOGCONFIG(TAG, "  R0: not available yet");
  }
  ESP_LOGCONFIG(TAG, "  RS/R0 in clean air: %.2f, correction factor: %.4f", this->ratio_in_clean_air_,
                this->correction_factor_);
  ESP_LOGCONFIG(TAG, "  Warm-up: %" PRIu32 " s, auto calibration: %s", this->warmup_time_ / 1000,
                this->calibration_enabled_ ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"));

  const bool tc_enabled = this->correction_mode_ == mqmath::CORRECTION_MQDATASCIENCE;
  ESP_LOGCONFIG(TAG, "  Temperature/humidity correction: %s",
                tc_enabled ? LOG_STR_LITERAL("mqdatascience (ratio / (a + c * exp(b * T)))") : LOG_STR_LITERAL("none"));
  if (tc_enabled) {
    ESP_LOGCONFIG(TAG, "  Correction constants (RH 33%%/85%%): a %.4f/%.4f, b %.4f/%.4f, c %.4f/%.4f", this->tc_.a33,
                  this->tc_.a85, this->tc_.b33, this->tc_.b85, this->tc_.c33, this->tc_.c85);
    ESP_LOGCONFIG(TAG, "  Correction sources: temperature %s, humidity %s, clamp: %s",
                  this->temperature_source_ != nullptr ? LOG_STR_LITERAL("wired") : LOG_STR_LITERAL("MISSING"),
                  this->humidity_source_ != nullptr ? LOG_STR_LITERAL("wired") : LOG_STR_LITERAL("MISSING"),
                  this->correction_clamp_ == mqmath::CLAMP_SCALED ? LOG_STR_LITERAL("max_ppm * correction")
                                                                  : LOG_STR_LITERAL("absolute max_ppm"));
  }
}

void MQGasSensor::dump_config() {
  LOG_SENSOR("", "MQ gas sensor", this);
  this->log_config_();
}

float MQGasSensor::sample_voltage_() {
  if (this->source_ == nullptr)
    return NAN;

  float sum = 0.0f;
  for (uint8_t i = 0; i < this->samples_; i++) {
    if (i > 0)
      delay(this->sample_interval_);
    sum += this->source_->sample();
  }
  return (sum / static_cast<float>(this->samples_)) * this->voltage_multiplier_;
}

float MQGasSensor::current_rs_() const { return mqmath::rs_from_voltage(this->sensor_voltage_, this->vcc_, this->rl_); }

float MQGasSensor::current_ratio_() const {
  return mqmath::ratio_from_rs(this->rs_, this->r0_, this->correction_factor_,
                               static_cast<mqmath::RatioMode>(this->ratio_mode_));
}

float MQGasSensor::compute_correction_() {
  if (this->correction_mode_ != mqmath::CORRECTION_MQDATASCIENCE)
    return 1.0f;

  if (this->temperature_source_ == nullptr || this->humidity_source_ == nullptr) {
    if (!this->warned_correction_) {
      this->warned_correction_ = true;
      this->log_message_(LOG_WARN, "temperature/humidity correction is enabled but a source is missing - "
                                   "publishing the uncorrected value");
    }
    return 1.0f;
  }

  // Sensor::state is NAN until the first valid publication, so a source that is
  // still booting (or has failed) simply leaves the reading uncorrected.
  const float temperature = this->temperature_source_->state;
  const float humidity = this->humidity_source_->state;
  if (!std::isfinite(temperature) || !std::isfinite(humidity)) {
    if (!this->warned_correction_) {
      this->warned_correction_ = true;
      this->log_message_(LOG_WARN,
                         "no valid temperature/humidity reading yet (T=%.1f, RH=%.1f) - "
                         "publishing the uncorrected value",
                         temperature, humidity);
    }
    return 1.0f;
  }

  this->warned_correction_ = false;
  return mqmath::correction_coefficient(humidity, temperature, this->tc_);
}

float MQGasSensor::read_ppm_() {
  const float ratio = mqmath::apply_correction(this->ratio_, this->correction_);
  const float ppm = mqmath::ppm_from_ratio(this->a_, this->b_, ratio,
                                           static_cast<mqmath::RegressionMethod>(this->regression_method_));
  if (!std::isfinite(ppm))
    return NAN;
  return mqmath::clamp_ppm_corrected(ppm, this->min_ppm_, this->max_ppm_, this->correction_,
                                     static_cast<mqmath::CorrectionClamp>(this->correction_clamp_));
}

void MQGasSensor::publish_diagnostics_() {
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(this->sensor_voltage_);
  if (this->rs_sensor_ != nullptr)
    this->rs_sensor_->publish_state(this->rs_);
  if (this->ratio_sensor_ != nullptr)
    this->ratio_sensor_->publish_state(this->ratio_);
  if (this->correction_sensor_ != nullptr)
    this->correction_sensor_->publish_state(this->correction_);
}

void MQGasSensor::update() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->is_failed())
    return;

  if (this->calibrating_) {
    // The persisted R0 is about to change: never keep a value that was computed
    // with the previous one.  The message also reaches `log_sensor:`.
    this->publish_state(NAN);
    this->log_message_(LOG_DEBUG, "update skipped, a calibration is running - the sensor must be in clean air");
    return;
  }

  if (this->calibration_pending_) {
    this->publish_state(NAN);
    return;
  }

  if (this->warmup_time_ > 0 && now < this->warmup_end_) {
    if (!this->warmup_notified_) {
      this->warmup_notified_ = true;
      this->publish_state(NAN);
      this->log_message_(LOG_INFO, "warming up, %" PRIu32 " s to go", (this->warmup_end_ - now) / 1000);
    }
    return;
  }

  if (!this->has_r0()) {
    if (!this->warned_no_r0_) {
      this->warned_no_r0_ = true;
      this->log_message_(LOG_WARN, "R0 unknown - publish 'calibration:' or 'r0:' in the configuration");
    }
    this->publish_state(NAN);
    return;
  }

  this->sensor_voltage_ = this->sample_voltage_();
  if (!std::isfinite(this->sensor_voltage_) || this->sensor_voltage_ <= 0.01f) {
    if (!this->warned_voltage_) {
      this->warned_voltage_ = true;
      this->log_message_(LOG_WARN,
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
  this->warned_voltage_ = false;

  this->rs_ = this->current_rs_();
  this->ratio_ = this->current_ratio_();
  this->correction_ = this->compute_correction_();
  const float ppm = this->read_ppm_();

  this->log_reading_(ppm);
  this->log_message_(LOG_DEBUG, "V=%.4f V, RS=%.4f kOhm, ratio=%.4f (correction=%.4f) -> %.1f ppm",
                     this->sensor_voltage_, this->rs_, this->ratio_, this->correction_, ppm);

  this->publish_diagnostics_();
  this->publish_state(ppm);
}

void MQGasSensor::set_calibration(bool enabled, float ratio_in_clean_air, uint32_t delay, uint32_t duration,
                                  uint32_t samples, bool persist) {
  this->calibration_enabled_ = enabled;
  this->ratio_in_clean_air_ = ratio_in_clean_air;
  this->calibration_delay_ = delay;
  this->calibration_duration_ = duration;
  this->calibration_samples_ = samples;
  this->persist_ = persist;
}

void MQGasSensor::request_calibration() {
  if (this->calibrating_) {
    this->log_message_(LOG_WARN, "calibration already running");
    return;
  }
  if (this->calibration_pending_) {
    const uint32_t now = millis();
    this->log_message_(LOG_WARN, "calibration already requested - starting in %" PRIu32 " s",
                       (this->calibration_due_ > now ? this->calibration_due_ - now : 0) / 1000);
    return;
  }
  if (!(this->ratio_in_clean_air_ > 0.0f)) {
    this->log_message_(LOG_ERROR, "cannot calibrate without a valid 'ratio_in_clean_air'");
    return;
  }

  const uint32_t now = millis();
  this->calibration_due_ = now;
  this->calibration_pending_ = true;
  // A request (button, on_boot lambda) must never capture an unstable RS, so it
  // is deferred until the warm-up/burn-in window of this sensor has ended.
  if (this->warmup_time_ > 0 && now < this->warmup_end_) {
    this->calibration_due_ = this->warmup_end_;
    this->log_message_(LOG_INFO, "calibration requested - waiting for the warm-up window to end (%" PRIu32 " s)",
                       (this->warmup_end_ - now) / 1000);
  } else {
    this->log_message_(LOG_INFO, "calibration requested - keep the sensor in clean air");
  }
}

void MQGasSensor::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->is_failed())
    return;

  if (this->calibration_pending_) {
    if (now >= this->calibration_due_)
      this->begin_calibration_();
    return;
  }

  if (!this->calibrating_)
    return;

  if (now - this->calibration_last_sample_ < this->sample_interval_)
    return;

  this->calibration_last_sample_ = now;
  this->process_calibration_();
}

void MQGasSensor::begin_calibration_() {
  this->calibration_pending_ = false;
  this->calibrating_ = true;
  this->calibration_start_ = App.get_loop_component_start_time();
  this->calibration_last_sample_ = this->calibration_start_;
  this->calibration_count_ = 0;
  this->calibration_attempts_ = 0;
  this->calibration_sum_ = 0.0f;

  this->log_message_(LOG_INFO,
                     "calibrating R0 (RS/R0 in clean air = %.2f), %" PRIu32 " samples - keep the sensor in clean air",
                     this->ratio_in_clean_air_, this->calibration_samples_);
}

void MQGasSensor::process_calibration_() {
  this->calibration_attempts_++;

  const float voltage = this->sample_voltage_();
  if (std::isfinite(voltage) && voltage > 0.01f) {
    this->sensor_voltage_ = voltage;
    this->rs_ = this->current_rs_();
    const float r0 = mqmath::r0_from_clean_air(this->rs_, this->ratio_in_clean_air_, this->correction_factor_);
    if (r0 > 0.0f) {
      this->calibration_sum_ += r0;
      this->calibration_count_++;
    }
  }

  const bool samples_done = this->calibration_attempts_ >= this->calibration_samples_;
  const bool timed_out = this->calibration_duration_ > 0 && (App.get_loop_component_start_time() -
                                                             this->calibration_start_) >= this->calibration_duration_;
  if (samples_done || timed_out)
    this->finish_calibration_();
}

void MQGasSensor::finish_calibration_() {
  this->calibrating_ = false;
  const uint32_t attempts = this->calibration_attempts_;
  const uint32_t valid = this->calibration_count_;

  if (valid == 0) {
    this->log_message_(LOG_ERROR,
                       "calibration failed, none of the %" PRIu32
                       " samples produced a valid RS - check the AO wiring and the supply",
                       attempts);
    return;
  }

  const float r0 = this->calibration_sum_ / static_cast<float>(valid);
  if (!std::isfinite(r0) || r0 <= 0.0f) {
    this->log_message_(LOG_ERROR, "calibration produced an invalid R0 (%.6f)", r0);
    return;
  }

  this->r0_ = r0;
  if (this->persist_)
    this->save_r0_();

  this->log_message_(LOG_INFO, "R0 = %.4f kOhm (%" PRIu32 "/%" PRIu32 " samples valid)%s", this->r0_, valid, attempts,
                     this->persist_ ? LOG_STR_LITERAL(", stored in flash") : "");
  if (!this->persist_) {
    this->log_message_(LOG_INFO, "hard-code 'r0: %.4f' in the YAML to skip the calibration at boot", this->r0_);
  }
}

void MQGasSensor::save_r0_() {
  if (!this->r0_pref_.save(&this->r0_)) {
    this->log_message_(LOG_WARN, "could not store R0 in flash");
    return;
  }
  if (!global_preferences->sync()) {
    this->log_message_(LOG_WARN, "R0 stored but the flash sync failed (will be written later)");
  }
  this->log_message_(LOG_DEBUG, "R0 %.4f kOhm saved", this->r0_);
}

bool MQGasSensor::load_r0_() {
  float stored = 0.0f;
  if (!this->r0_pref_.load(&stored))
    return false;
  if (!std::isfinite(stored) || stored <= 0.0f)
    return false;
  this->r0_ = stored;
  return true;
}

}  // namespace esphome::mq_gas_sensors
