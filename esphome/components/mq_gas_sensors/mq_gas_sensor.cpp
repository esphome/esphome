#include "mq_gas_sensor.h"

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mq_gas_sensors {

ESPHOME_LOG_TAG(TAG, "mq_gas_sensors");

/// Version tag mixed into the preference key of the persisted R0 value.
static constexpr uint32_t R0_PREFERENCE_VERSION = 0x00000001;

/// Human readable name of a `mqmath::RegressionMethod` value.
static const LogString *regression_method_name(uint8_t method) {
  switch (method) {
    case mqmath::REGRESSION_LINEAR:
      return LOG_STR("linear");
    case mqmath::REGRESSION_INVERSE:
      return LOG_STR("inverse");
    default:
      return LOG_STR("exponential");
  }
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
  this->r0_pref_ = this->make_entity_preference<float>(R0_PREFERENCE_VERSION);

  if (this->r0_configured_) {
    this->log_message_(LOG_SEVERITY_INFO, "using the R0 from the configuration (%.4f kOhm)", this->r0_);
  } else if (this->persist_ && this->load_r0_()) {
    this->log_message_(LOG_SEVERITY_INFO, "restored R0 from flash (%.4f kOhm)", this->r0_);
  }

  if (this->warmup_time_ > 0) {
    this->warmup_start_ = App.get_loop_component_start_time();
    this->log_message_(LOG_SEVERITY_INFO, "warm-up/burn-in of %" PRIu32 " s - no readings published before that",
                       this->warmup_time_ / 1000);
  }

  if (this->calibration_enabled_) {
    if (this->has_r0()) {
      this->log_message_(LOG_SEVERITY_INFO,
                         "R0 already known (%.4f kOhm) - automatic calibration skipped (clear the stored R0 to "
                         "recalibrate)",
                         this->r0_);
    } else {
      // The warm-up window starts here, so all of it still lies ahead of the calibration.
      const uint32_t delay = std::max(this->calibration_delay_, this->warmup_time_);
      this->calibration_pending_ = true;
      this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
      this->log_message_(LOG_SEVERITY_INFO,
                         "R0 calibration scheduled in %" PRIu32 " s - the sensor must be in clean air", delay / 1000);
    }
  } else if (!this->has_r0()) {
    this->log_message_(LOG_SEVERITY_WARN, "no R0 available, set 'r0:' or 'calibration:' - the PPM value stays "
                                          "unknown until then");
  }

  // The loop only samples while a clean-air calibration runs; `update()` publishes.
  this->disable_loop();
}

void MQGasSensor::log_config_() {
  ESP_LOGCONFIG(TAG, "  Type: %s", this->type_.c_str());
  ESP_LOGCONFIG(TAG, "  Target gas: %s", this->gas_.c_str());
  ESP_LOGCONFIG(TAG, "  Curve: %s (a=%.6g, b=%.6g), ratio: %s",
                LOG_STR_ARG(regression_method_name(this->regression_method_)), this->a_, this->b_,
                this->ratio_mode_ == mqmath::RATIO_R0_RS ? LOG_STR_LITERAL("R0/RS (MQUnifiedsensor)")
                                                         : LOG_STR_LITERAL("RS/R0 (datasheet)"));
  ESP_LOGCONFIG(TAG, "  VCC: %.2f V, RL: %.2f kOhm, AO multiplier: x%.3f", this->vcc_, this->rl_,
                this->voltage_multiplier_);
  ESP_LOGCONFIG(TAG, "  Range: %.1f - %.1f ppm, AO samples per reading: %u, calibration spacing: %" PRIu32 " ms",
                this->min_ppm_, this->max_ppm_, static_cast<unsigned>(this->samples_), this->sample_interval_);
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
  LOG_UPDATE_INTERVAL(this);
  this->log_config_();
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

float MQGasSensor::compute_correction_() {
  if (this->correction_mode_ != mqmath::CORRECTION_MQDATASCIENCE)
    return 1.0f;

  if (this->temperature_source_ == nullptr || this->humidity_source_ == nullptr) {
    if (!this->warned_correction_) {
      this->warned_correction_ = true;
      this->log_message_(LOG_SEVERITY_WARN, "temperature/humidity correction is enabled but a source is missing - "
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
      this->log_message_(LOG_SEVERITY_WARN,
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
  if (this->is_failed())
    return;

  if (this->calibrating_) {
    // The persisted R0 is about to change: never keep a value that was computed
    // with the previous one.  The message also reaches `log_sensor:`.
    this->publish_state(NAN);
    this->log_message_(LOG_SEVERITY_DEBUG,
                       "update skipped, a calibration is running - the sensor must be in clean air");
    return;
  }

  if (this->calibration_pending_) {
    this->publish_state(NAN);
    return;
  }

  if (this->in_warmup_()) {
    // Published on every pass, so a value computed before a restart never survives
    // the warm-up window. `setup()` logged the window, so nothing repeats here.
    this->publish_state(NAN);
    return;
  }

  if (!this->has_r0()) {
    if (!this->warned_no_r0_) {
      this->warned_no_r0_ = true;
      this->log_message_(LOG_SEVERITY_WARN, "R0 unknown - publish 'calibration:' or 'r0:' in the configuration");
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
  this->correction_ = this->compute_correction_();
  const float ppm = this->read_ppm_();

  this->log_reading_(ppm);
  this->log_message_(LOG_SEVERITY_DEBUG, "V=%.4f V, RS=%.4f kOhm, ratio=%.4f (correction=%.4f) -> %.1f ppm",
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

void MQGasSensor::loop() {
  // The loop runs only while a calibration samples; the deferred start is a timer.
  if (this->is_failed() || !this->calibrating_)
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
  this->calibration_start_ = App.get_loop_component_start_time();
  this->calibration_last_sample_ = this->calibration_start_;
  this->calibration_count_ = 0;
  this->calibration_attempts_ = 0;
  this->calibration_sum_ = 0.0f;
  this->calibration_min_ = 0.0f;
  this->calibration_max_ = 0.0f;
  this->enable_loop();

  this->log_message_(LOG_SEVERITY_INFO,
                     "calibrating R0 (RS/R0 in clean air = %.2f), %" PRIu32 " samples - keep the sensor in clean air",
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
      if (this->calibration_count_ == 0) {
        this->calibration_min_ = r0;
        this->calibration_max_ = r0;
      } else {
        this->calibration_min_ = std::min(this->calibration_min_, r0);
        this->calibration_max_ = std::max(this->calibration_max_, r0);
      }
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
  this->disable_loop();

  const uint32_t attempts = this->calibration_attempts_;
  const uint32_t valid = this->calibration_count_;
  // Every reading uses this R0, so a lucky minority of samples must not define it:
  // a majority of the attempts has to have produced one, two samples at least.
  const uint32_t required = std::max<uint32_t>((attempts + 1) / 2, 2);

  if (valid < required) {
    this->log_message_(LOG_SEVERITY_ERROR,
                       "calibration failed, %" PRIu32 " of %" PRIu32
                       " samples produced a valid RS - check the AO wiring and the supply",
                       valid, attempts);
    this->schedule_calibration_retry_();
    return;
  }

  // Every counted sample is finite and positive (`process_calibration_()`), so their
  // average is usable without another check.
  const float r0 = this->calibration_sum_ / static_cast<float>(valid);

  // A sensor that is still settling, or one that is not in clean air, varies too
  // much for its average to be the R0 of every later reading.
  if (!mqmath::calibration_is_stable(this->calibration_min_, this->calibration_max_, r0)) {
    this->log_message_(LOG_SEVERITY_WARN,
                       "clean-air samples spread %.4f kOhm around %.4f kOhm - the sensor was not settled in clean air",
                       this->calibration_max_ - this->calibration_min_, r0);
    this->schedule_calibration_retry_();
    return;
  }

  this->r0_ = r0;
  const bool stored = this->persist_ && this->save_r0_();
  if (this->persist_ && !stored) {
    // The reading stays usable, but the next boot calibrates again: keep the
    // failure visible instead of clearing the warning of the failed attempts.
    this->status_set_warning(LOG_STR("could not store R0 in flash"));
  } else {
    this->status_clear_warning();
  }

  this->log_message_(LOG_SEVERITY_INFO, "R0 = %.4f kOhm (%" PRIu32 "/%" PRIu32 " samples valid)%s", this->r0_, valid,
                     attempts,
                     stored           ? LOG_STR_LITERAL(", stored in flash")
                     : this->persist_ ? LOG_STR_LITERAL(", not stored in flash")
                                      : LOG_STR_LITERAL(", kept until reboot"));
  if (!this->persist_) {
    this->log_message_(LOG_SEVERITY_INFO, "hard-code 'r0: %.4f' in the YAML to skip the calibration at boot",
                       this->r0_);
  }
}

void MQGasSensor::schedule_calibration_retry_() {
  this->status_set_warning();
  this->calibration_pending_ = true;
  // A delay of zero restarts the run straight away and logs the same failure again
  // about five times per second, so anything below the floor is raised to it.
  const uint32_t delay = std::max(this->calibration_delay_, CALIBRATION_RETRY_MIN_MS);
  this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
  this->log_message_(LOG_SEVERITY_WARN, "retrying the calibration in %" PRIu32 " s", delay / 1000);
}

bool MQGasSensor::in_warmup_() {
  if (this->warmup_ended_)
    return false;
  if (App.get_loop_component_start_time() - this->warmup_start_ < this->warmup_time_)
    return true;
  // A window that has passed is over: latching it keeps the subtraction above from
  // reopening it once it overflows, ~49.7 days after the start.
  this->warmup_ended_ = true;
  return false;
}

bool MQGasSensor::save_r0_() {
  if (!this->r0_pref_.save(&this->r0_)) {
    this->log_message_(LOG_SEVERITY_WARN, "could not store R0 in flash");
    return false;
  }
  if (!global_preferences->sync()) {
    this->log_message_(LOG_SEVERITY_WARN, "R0 stored but the flash sync failed (will be written later)");
  }
  this->log_message_(LOG_SEVERITY_DEBUG, "R0 %.4f kOhm saved", this->r0_);
  return true;
}

bool MQGasSensor::load_r0_() {
  float stored = 0.0f;
  if (!this->r0_pref_.load(&stored))
    return false;
  if (!std::isfinite(stored) || stored <= 0.0f) {
    this->log_message_(LOG_SEVERITY_WARN, "ignoring the stored R0 %.4f, calibrating again", stored);
    return false;
  }
  this->r0_ = stored;
  return true;
}

}  // namespace esphome::mq_gas_sensors
