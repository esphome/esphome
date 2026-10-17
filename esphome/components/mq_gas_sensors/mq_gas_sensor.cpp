#include "mq_gas_sensor.h"

#include <algorithm>
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mq_gas_sensors {

ESPHOME_LOG_TAG(TAG, "mq_gas_sensors");

/// Human readable name of a `mqmath::RegressionMethod` value.
static const LogString *regression_method_name(uint8_t method) {
  if (method == mqmath::REGRESSION_LINEAR)
    return LOG_STR("linear");
  if (method == mqmath::REGRESSION_INVERSE)
    return LOG_STR("inverse");
  return LOG_STR("exponential");
}

void MQGasSensor::setup() {
  if (this->r0_configured_) {
    ESP_LOGI(TAG, "'%s %s': using the R0 from the configuration (%.4f kOhm)", this->type_.c_str(), this->gas_.c_str(),
             this->r0_);
  }

  if (this->calibration_enabled_) {
    if (this->has_r0()) {
      ESP_LOGI(TAG, "'%s %s': R0 already known (%.4f kOhm) - automatic calibration skipped", this->type_.c_str(),
               this->gas_.c_str(), this->r0_);
    } else {
      this->calibration_pending_ = true;
      this->set_timeout(CALIBRATION_START_ID, this->calibration_delay_, [this]() { this->begin_calibration_(); });
      ESP_LOGI(TAG, "'%s %s': R0 calibration in %" PRIu32 " s - the sensor must be in clean air", this->type_.c_str(),
               this->gas_.c_str(), this->calibration_delay_ / 1000);
    }
  } else if (!this->has_r0()) {
    ESP_LOGW(TAG, "'%s %s': no R0 available, set 'r0:' or 'calibration:' - the PPM value stays unknown until then",
             this->type_.c_str(), this->gas_.c_str());
  }

  // The loop only samples while a clean-air calibration runs; `update()` publishes.
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
  // The conversions are taken back to back: the sampler averages internally as
  // configured, and waiting between them would block the main loop.
  float sum = 0.0f;
  for (uint8_t i = 0; i < this->samples_; i++)
    sum += this->source_->sample();
  return (sum / static_cast<float>(this->samples_)) * this->voltage_multiplier_;
}

float MQGasSensor::current_rs_(float voltage) const { return mqmath::rs_from_voltage(voltage, this->vcc_, this->rl_); }

float MQGasSensor::current_ratio_(float rs) const {
  return mqmath::ratio_from_rs(rs, this->r0_, 0.0f, mqmath::RATIO_RS_R0);
}

float MQGasSensor::read_ppm_(float ratio) const {
  const float ppm = mqmath::ppm_from_ratio(this->a_, this->b_, ratio,
                                           static_cast<mqmath::RegressionMethod>(this->regression_method_));
  if (!std::isfinite(ppm))
    return NAN;
  return mqmath::clamp_ppm(ppm, this->min_ppm_, this->max_ppm_);
}

void MQGasSensor::update() {
  if (this->is_failed())
    return;

  if (this->calibrating_) {
    // The R0 is about to change: never keep a value computed with the previous one.
    this->publish_state(NAN);
    ESP_LOGD(TAG, "'%s %s': update skipped, a calibration is running - the sensor must be in clean air",
             this->type_.c_str(), this->gas_.c_str());
    return;
  }

  if (this->calibration_pending_) {
    this->publish_state(NAN);
    return;
  }

  if (!this->has_r0()) {
    if (!this->warned_no_r0_) {
      this->warned_no_r0_ = true;
      ESP_LOGW(TAG, "'%s %s': R0 unknown - %s", this->type_.c_str(), this->gas_.c_str(),
               this->calibration_enabled_
                   ? LOG_STR_LITERAL("the clean-air calibration did not complete, check the AO wiring and the supply")
                   : LOG_STR_LITERAL("set 'r0:' or 'calibration:' in the configuration"));
    }
    this->publish_state(NAN);
    return;
  }

  const float voltage = this->sample_voltage_();
  if (!std::isfinite(voltage) || voltage <= 0.01f) {
    if (!this->warned_voltage_) {
      this->warned_voltage_ = true;
      ESP_LOGW(TAG, "'%s %s': analog output reads %.4f V - open circuit, missing supply or bad wiring?",
               this->type_.c_str(), this->gas_.c_str(), voltage);
    }
    this->publish_state(NAN);
    return;
  }
  if (voltage >= this->vcc_) {
    // Only a wrong `vcc:` / `voltage_multiplier:` reads at or above the supply, and
    // reading it as the saturated end of the range would turn a configuration error
    // into what looks like a real measurement.
    if (!this->warned_voltage_) {
      this->warned_voltage_ = true;
      ESP_LOGW(TAG,
               "'%s %s': analog output reads %.4f V, at or above the configured vcc (%.2f V) - check 'vcc:' and "
               "'voltage_multiplier:'",
               this->type_.c_str(), this->gas_.c_str(), voltage, this->vcc_);
    }
    this->publish_state(NAN);
    return;
  }
  this->warned_voltage_ = false;

  const float rs = this->current_rs_(voltage);
  const float ratio = this->current_ratio_(rs);
  const float ppm = this->read_ppm_(ratio);

  ESP_LOGD(TAG, "'%s %s': V=%.4f V, RS=%.4f kOhm, ratio=%.4f -> %.1f ppm", this->type_.c_str(), this->gas_.c_str(),
           voltage, rs, ratio, ppm);
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

  ESP_LOGI(TAG,
           "'%s %s': calibrating R0 (RS/R0 in clean air = %.2f), %" PRIu32 " samples - keep the sensor in clean air",
           this->type_.c_str(), this->gas_.c_str(), this->ratio_in_clean_air_, this->calibration_samples_);
}

void MQGasSensor::process_calibration_() {
  this->calibration_attempts_++;

  const float voltage = this->sample_voltage_();
  if (std::isfinite(voltage) && voltage > 0.01f && voltage < this->vcc_) {
    const float r0 = mqmath::r0_from_clean_air(this->current_rs_(voltage), this->ratio_in_clean_air_, 0.0f);
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
  // Every reading uses this R0, so a lucky minority of samples must not define it: at
  // least half of the attempts has to have produced one, and at least two samples - or
  // every sample, when the run took fewer than two.
  const uint32_t required = std::min<uint32_t>(std::max<uint32_t>((attempts + 1) / 2, 2), attempts);

  if (valid < required) {
    ESP_LOGE(TAG,
             "'%s %s': calibration failed, %" PRIu32 " of %" PRIu32
             " samples produced a valid RS - check the AO wiring and the supply",
             this->type_.c_str(), this->gas_.c_str(), valid, attempts);
    this->status_set_warning(LOG_STR("clean-air calibration failed"));
    return;
  }

  this->r0_ = this->calibration_sum_ / static_cast<float>(valid);

  ESP_LOGI(TAG, "'%s %s': R0 = %.4f kOhm (%" PRIu32 "/%" PRIu32 " samples valid)", this->type_.c_str(),
           this->gas_.c_str(), this->r0_, valid, attempts);
  ESP_LOGI(TAG, "'%s %s': hard-code 'r0: %.4f' in the YAML to skip the calibration at boot", this->type_.c_str(),
           this->gas_.c_str(), this->r0_);
}

}  // namespace esphome::mq_gas_sensors
