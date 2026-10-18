#include "mq_gas_sensor.h"

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mq_gas_sensors {

ESPHOME_LOG_TAG(TAG, "mq_gas_sensors");

/// A sensor whose resistance collapses drives its analog output to its supply, and
/// the supply rail can sit this much above the configured `vcc:` (a 5.1 V USB rail
/// against the default 5.0 V). A reading within the tolerance is the top of the
/// measuring range; above it the reading cannot come from the configured sensor.
static constexpr float SATURATION_TOLERANCE = 0.05f;

/// Human readable name of a `mqmath::RegressionMethod` value.
static const LogString *regression_method_name(uint8_t method) {
  if (method == mqmath::REGRESSION_LINEAR)
    return LOG_STR("linear");
  if (method == mqmath::REGRESSION_INVERSE)
    return LOG_STR("inverse");
  return LOG_STR("exponential");
}

#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
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
#endif

void MQGasSensor::log_message_(LogSeverity level, const LogString *format, ...) {
  char message[LOG_BUFFER_SIZE];
  // A failed `snprintf` leaves the buffer untouched, and the message below is both
  // logged and published as a string, so it must never be uninitialized.
  message[0] = '\0';
  const int prefix = std::snprintf(message, sizeof(message), "'%s %s': ", this->type_.c_str(), this->gas_.c_str());
  // A failed or truncated prefix must not take the message with it: the body is what
  // carries the fault, so it is formatted from the start of the buffer when the
  // prefix does not leave room for it.
  size_t used = 0;
  if (prefix > 0 && static_cast<size_t>(prefix) < sizeof(message) - 1) {
    used = static_cast<size_t>(prefix);
  } else {
    message[0] = '\0';
  }
  va_list args;
  va_start(args, format);
#ifdef USE_ESP8266
  // The format is in flash (see LOG_STR), so it is read from there with the
  // `_P` variant - the same call the logger itself makes.
  vsnprintf_P(message + used, sizeof(message) - used, LOG_STR_ARG(format), args);
#else
  std::vsnprintf(message + used, sizeof(message) - used, LOG_STR_ARG(format), args);
#endif
  va_end(args);

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

#ifdef USE_MQ_GAS_SENSORS_LOG_SENSOR
  this->publish_log_(level, message);
#endif
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
    this->log_message_(LOG_SEVERITY_INFO, LOG_STR("using the R0 from the configuration (%.4f kOhm)"), this->r0_);
  }

  if (this->calibration_enabled_) {
    if (this->has_r0()) {
      this->log_message_(LOG_SEVERITY_INFO, LOG_STR("R0 already known (%.4f kOhm) - automatic calibration skipped"),
                         this->r0_);
    } else {
      this->calibration_pending_ = true;
      this->set_timeout(CALIBRATION_START_ID, this->calibration_delay_, [this]() { this->begin_calibration_(); });
      this->log_message_(LOG_SEVERITY_INFO,
                         LOG_STR("R0 calibration in %" PRIu32 " s - the sensor must be in clean air"),
                         this->calibration_delay_ / 1000);
    }
  } else if (!this->has_r0()) {
    // No R0 will ever appear in this configuration, so the component is degraded for
    // its whole life: keep the status warning up, not only the log line.
    this->r0_warning_ = LOG_STR("no R0 available - set 'r0:' or 'calibration:' in the configuration");
    this->status_set_warning(this->r0_warning_);
    this->log_message_(
        LOG_SEVERITY_WARN,
        LOG_STR("no R0 available, set 'r0:' or 'calibration:' - the PPM value stays unknown until then"));
  }

  this->disable_loop();
}

void MQGasSensor::dump_config() {
  LOG_SENSOR("", "MQ gas sensor", this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Type: %s, target gas: %s", this->type_.c_str(), this->gas_.c_str());
  ESP_LOGCONFIG(TAG, "  Curve: %s (a=%.6g, b=%.6g)", LOG_STR_ARG(regression_method_name(this->regression_method_)),
                this->a_, this->b_);
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

void MQGasSensor::publish_fault_diagnostics_() {
  // The voltage is a real reading even when no resistance can be derived from
  // it; RS and the ratio are not, and 0 kOhm would look like a measurement.
  this->rs_ = NAN;
  this->ratio_ = NAN;
  this->publish_diagnostics_();
}

void MQGasSensor::update() {
  if (this->is_failed())
    return;

  if (this->calibrating_) {
    // The R0 is about to change: never keep a value that was computed with the
    // previous one. The message also reaches `log_sensor:`.
    this->publish_state(NAN);
    this->log_message_(LOG_SEVERITY_DEBUG,
                       LOG_STR("update skipped, a calibration is running - the sensor must be in clean air"));
    return;
  }

  if (this->calibration_pending_) {
    this->publish_state(NAN);
    return;
  }

  this->sensor_voltage_ = this->sample_voltage_();
  if (!std::isfinite(this->sensor_voltage_) || this->sensor_voltage_ <= 0.01f) {
    if (!this->warned_low_voltage_) {
      this->warned_low_voltage_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         LOG_STR("analog output reads %.4f V - open circuit, missing supply or bad "
                                 "divider? check the AO wiring (RS would be invalid)"),
                         this->sensor_voltage_);
      this->wiring_warning_ = true;
    }
    // Raised on every update while the fault lasts, not only when it appears: any
    // other warning that cleared the status must not hide it.
    this->status_set_warning(LOG_STR("analog output reads at or below 0.01 V - check the AO wiring"));
    this->publish_fault_diagnostics_();
    this->publish_state(NAN);
    return;
  }
  bool saturated = false;
  if (this->sensor_voltage_ >= this->vcc_) {
    // A sensor whose resistance collapses drives its output to the supply, and the
    // supply itself can sit a few percent above the configured `vcc:`. Up to the
    // tolerance that is the top of the measuring range; beyond it the reading cannot
    // come from the sensor the configuration describes.
    saturated = this->sensor_voltage_ <= this->vcc_ * (1.0f + SATURATION_TOLERANCE);
    if (!saturated) {
      if (!this->warned_high_voltage_) {
        this->warned_high_voltage_ = true;
        this->log_message_(LOG_SEVERITY_WARN,
                           LOG_STR("analog output reads %.4f V, above the configured vcc (%.2f V) by more than "
                                   "%.0f %% - check 'vcc:' and the divider"),
                           this->sensor_voltage_, this->vcc_, SATURATION_TOLERANCE * 100.0f);
        this->wiring_warning_ = true;
      }
      // See the low-voltage branch: the status warning is a state, not an event.
      this->status_set_warning(LOG_STR("analog output is far above vcc - check 'vcc:' and the divider"));
      this->publish_fault_diagnostics_();
      this->publish_state(NAN);
      return;
    }
  }
  // The wiring reads as expected again, so both voltage faults are over and the
  // status warning raised for them goes away with them. A warning raised for the
  // ratio (below) or by a failed calibration is not this component's to clear
  // here, so only a wiring warning is cleared.
  this->warned_low_voltage_ = false;
  this->warned_high_voltage_ = false;
  if (this->wiring_warning_) {
    this->wiring_warning_ = false;
    this->refresh_status_warning_();
  }

  if (!this->has_r0()) {
    if (!this->warned_no_r0_) {
      this->warned_no_r0_ = true;
      this->log_message_(LOG_SEVERITY_WARN, LOG_STR("R0 unknown - %s"),
                         this->calibration_enabled_
                             ? LOG_STR_LITERAL("the clean-air calibration did not complete, check the AO wiring and "
                                               "the supply")
                             : LOG_STR_LITERAL("set 'r0:' or 'calibration:' in the configuration"));
    }
    // The voltage and RS are real readings even without R0, so the diagnostics
    // that show the wiring keep working; the ratio and the ppm need R0 and stay
    // unknown until it is known. At the supply rail RS has collapsed, so the same 0
    // a saturated reading with R0 reports is used instead of the negative RS that
    // `current_rs_()` would derive from the voltage.
    this->rs_ = saturated ? 0.0f : this->current_rs_();
    this->ratio_ = NAN;
    this->publish_diagnostics_();
    this->publish_state(NAN);
    return;
  }

  if (saturated) {
    // RS has collapsed to 0, so the ratio is 0 and the curve reports the top of its
    // range: a real high concentration must not look like a broken sensor.
    this->rs_ = 0.0f;
    this->ratio_ = 0.0f;
    // A ratio fault from an earlier update is over: this is a valid reading.
    this->clear_ratio_warning_();
    this->log_reading_(this->max_ppm_);
    this->log_message_(LOG_SEVERITY_DEBUG,
                       LOG_STR("V=%.4f V is at the supply - the sensor is saturated, reporting the top of the range"),
                       this->sensor_voltage_);
    this->publish_diagnostics_();
    this->publish_state(this->max_ppm_);
    return;
  }

  this->rs_ = this->current_rs_();
  this->ratio_ = this->current_ratio_();
  const float ppm = this->read_ppm_();

  if (!std::isfinite(ppm)) {
    // The wiring reads fine but the ratio is unusable: the curve cannot evaluate
    // it for the coefficients and mode in use. Publishing unknown in silence would
    // look like a sensor that simply reads nothing.
    if (!this->warned_unusable_ratio_) {
      this->warned_unusable_ratio_ = true;
      this->log_message_(LOG_SEVERITY_WARN,
                         LOG_STR("V=%.4f V, RS=%.4f kOhm gives ratio %.4f, which the curve cannot evaluate - check "
                                 "'correction_factor:' and the coefficients"),
                         this->sensor_voltage_, this->rs_, this->ratio_);
    }
    // Raised on every update, not only the first one: every reading is unknown for
    // as long as the ratio stays unusable, and nothing else may report the
    // component as healthy while that is the case.
    this->status_set_warning(
        LOG_STR("the ratio is not a measurement - check 'correction_factor:' and the coefficients"));
    this->publish_diagnostics_();
    this->publish_state(NAN);
    return;
  }
  this->clear_ratio_warning_();

  this->log_reading_(ppm);
  this->log_message_(LOG_SEVERITY_DEBUG, LOG_STR("V=%.4f V, RS=%.4f kOhm, ratio=%.4f -> %.1f ppm"),
                     this->sensor_voltage_, this->rs_, this->ratio_, ppm);

  this->publish_diagnostics_();
  this->publish_state(ppm);
}

void MQGasSensor::refresh_status_warning_() {
  // The status slot holds one warning: when a fault that raised it ends, a warning
  // that is still current has to be restated.
  if (this->r0_warning_ != nullptr) {
    this->status_set_warning(this->r0_warning_);
  } else {
    this->status_clear_warning();
  }
}

void MQGasSensor::clear_ratio_warning_() {
  // A valid reading ends a ratio fault from an earlier update. The saturated branch
  // publishes the top of the range without going through the curve, so it has to
  // clear the fault itself.
  if (!this->warned_unusable_ratio_)
    return;
  this->warned_unusable_ratio_ = false;
  this->refresh_status_warning_();
}

void MQGasSensor::set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval) {
  this->calibration_enabled_ = true;
  this->calibration_delay_ = delay;
  this->calibration_samples_ = samples;
  this->sample_interval_ = sample_interval;
}

void MQGasSensor::loop() {
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

  this->log_message_(
      LOG_SEVERITY_INFO,
      LOG_STR("calibrating R0 (RS/R0 in clean air = %.2f), %" PRIu32 " samples - keep the sensor in clean air"),
      this->ratio_in_clean_air_, this->calibration_samples_);
}

void MQGasSensor::process_calibration_() {
  this->calibration_attempts_++;

  const float voltage = this->sample_voltage_();
  if (std::isfinite(voltage) && voltage > 0.01f && voltage < this->vcc_) {
    this->sensor_voltage_ = voltage;
    this->rs_ = this->current_rs_();
    const float r0 = mqmath::r0_from_clean_air(this->rs_, this->ratio_in_clean_air_);
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
  const uint32_t required = std::min<uint32_t>(std::max<uint32_t>((attempts + 1) / 2, 2), attempts);

  if (valid < required) {
    this->log_message_(LOG_SEVERITY_ERROR,
                       LOG_STR("calibration failed, %" PRIu32 " of %" PRIu32
                               " samples produced a valid RS - check the AO wiring and the supply"),
                       valid, attempts);
    this->r0_warning_ = LOG_STR("clean-air calibration failed");
    this->status_set_warning(this->r0_warning_);
    return;
  }

  this->r0_ = this->calibration_sum_ / static_cast<float>(valid);

  this->log_message_(LOG_SEVERITY_INFO, LOG_STR("R0 = %.4f kOhm (%" PRIu32 "/%" PRIu32 " samples valid)"), this->r0_,
                     valid, attempts);
  this->log_message_(LOG_SEVERITY_INFO, LOG_STR("hard-code 'r0: %.4f' in the YAML to skip the calibration at boot"),
                     this->r0_);
}

}  // namespace esphome::mq_gas_sensors
