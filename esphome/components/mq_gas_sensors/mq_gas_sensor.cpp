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
  } else if (this->calibration_enabled_) {
    this->calibration_pending_ = true;
    this->set_timeout(CALIBRATION_START_ID, this->calibration_delay_, [this]() { this->begin_calibration_(); });
    ESP_LOGI(TAG, "'%s %s': R0 calibration in %" PRIu32 " s - the sensor must be in clean air", this->type_.c_str(),
             this->gas_.c_str(), this->calibration_delay_ / 1000);
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

float MQGasSensor::current_rs_(float voltage) const { return mqmath::rs_from_voltage(voltage, this->vcc_, this->rl_); }

float MQGasSensor::current_ratio_(float rs) const { return mqmath::ratio_from_rs(rs, this->r0_); }

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
    // Exactly one of `r0:` and `calibration:` is configured, so an unknown R0 at
    // this point is a boot calibration that has not succeeded; it logged the cause.
    this->status_set_warning(LOG_STR("clean-air calibration failed - check the AO wiring and the supply"));
    this->publish_state(NAN);
    return;
  }

  const float voltage = this->sample_voltage_();
  if (!this->voltage_usable_(voltage)) {
    if (!this->warned_low_voltage_) {
      this->warned_low_voltage_ = true;
      ESP_LOGW(TAG, "'%s %s': analog output reads %.4f V - open circuit, missing supply or bad wiring?",
               this->type_.c_str(), this->gas_.c_str(), voltage);
    }
    // Raised on every update while the fault lasts, not only when it appears: a
    // status warning is a state, not an event.
    this->status_set_warning(LOG_STR("analog output reads at or below 0.01 V - check the AO wiring"));
    this->publish_state(NAN);
    return;
  }
  bool saturated = false;
  if (voltage >= this->vcc_) {
    // A sensor whose resistance collapses drives its output to the supply, and the
    // supply itself can sit a few percent above the configured `vcc:`. Up to the
    // tolerance that is the top of the measuring range; beyond it the reading cannot
    // come from the sensor the configuration describes.
    saturated = voltage <= this->vcc_ * (1.0f + SATURATION_TOLERANCE);
    if (!saturated) {
      if (!this->warned_high_voltage_) {
        this->warned_high_voltage_ = true;
        ESP_LOGW(TAG, "'%s %s': analog output reads %.4f V, above vcc (%.2f V) by more than %.0f %% - check 'vcc:'",
                 this->type_.c_str(), this->gas_.c_str(), voltage, this->vcc_, SATURATION_TOLERANCE * 100.0f);
      }
      this->status_set_warning(LOG_STR("analog output is far above vcc - check 'vcc:' and 'voltage_multiplier:'"));
      this->publish_state(NAN);
      return;
    }
  }
  this->warned_low_voltage_ = false;
  this->warned_high_voltage_ = false;

  // At the supply, RS has collapsed to 0 and the ratio is 0, so the curve reports
  // the top of its range: a real high concentration must not look like a fault.
  const float rs = saturated ? 0.0f : this->current_rs_(voltage);
  const float ratio = saturated ? 0.0f : this->current_ratio_(rs);
  const float ppm = saturated ? this->max_ppm_ : this->read_ppm_(ratio);

  if (!std::isfinite(ppm)) {
    // The wiring reads fine but the curve cannot evaluate the ratio: publishing
    // unknown in silence would look like a sensor that simply reads nothing.
    if (!this->warned_curve_failure_) {
      this->warned_curve_failure_ = true;
      ESP_LOGW(TAG, "'%s %s': V=%.4f V, RS=%.4f kOhm gives ratio %.4f, which the curve cannot evaluate",
               this->type_.c_str(), this->gas_.c_str(), voltage, rs, ratio);
    }
    // Raised on every update, not only the first one: every reading is unknown for
    // as long as the ratio stays unusable.
    this->status_set_warning(LOG_STR("PPM curve evaluation failed - check the coefficients"));
    this->publish_state(NAN);
    return;
  }
  this->warned_curve_failure_ = false;
  this->status_clear_warning();

  // The published value is clamped to the range and a saturated sensor reports its
  // ceiling, so say so once: neither may pass for a measurement at that value.
  const bool at_ceiling = ppm >= this->max_ppm_;
  if (at_ceiling != this->warned_over_range_) {
    this->warned_over_range_ = at_ceiling;
    if (at_ceiling) {
      ESP_LOGW(TAG, "'%s %s': V=%.4f V is beyond %.1f ppm - reporting the top of the range", this->type_.c_str(),
               this->gas_.c_str(), voltage, this->max_ppm_);
    }
  }

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
  this->calibration_rejected_low_ = 0;
  this->calibration_rejected_high_ = 0;
  this->calibration_rejected_nonfinite_ = 0;
  this->calibration_rejected_rs_ = 0;
  this->calibration_sum_ = 0.0f;
  this->enable_loop();

  ESP_LOGI(TAG,
           "'%s %s': calibrating R0 (RS/R0 in clean air = %.2f), %" PRIu32 " samples - keep the sensor in clean air",
           this->type_.c_str(), this->gas_.c_str(), this->ratio_in_clean_air_, this->calibration_samples_);
}

void MQGasSensor::process_calibration_() {
  this->calibration_attempts_++;

  const float voltage = this->sample_voltage_();
  if (!std::isfinite(voltage)) {
    this->calibration_rejected_nonfinite_++;
  } else if (!this->voltage_usable_(voltage)) {
    this->calibration_rejected_low_++;
  } else if (voltage >= this->vcc_) {
    this->calibration_rejected_high_++;
  } else {
    const float r0 = mqmath::r0_from_clean_air(this->current_rs_(voltage), this->ratio_in_clean_air_);
    if (r0 > 0.0f) {
      this->calibration_sum_ += r0;
      this->calibration_count_++;
    } else {
      this->calibration_rejected_rs_++;
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
    // Name the reason each rejected sample failed, so a wrong `vcc:` is not
    // mistaken for a wiring fault just because the generic message says so.
    ESP_LOGE(TAG,
             "'%s %s': calibration failed, %" PRIu32 " of %" PRIu32 " valid RS: %" PRIu32 " below %.2f V, %" PRIu32
             " at/above vcc, %" PRIu32 " non-finite, %" PRIu32 " bad RS - check AO wiring and supply",
             this->type_.c_str(), this->gas_.c_str(), valid, attempts, this->calibration_rejected_low_,
             MIN_VALID_VOLTAGE, this->calibration_rejected_high_, this->calibration_rejected_nonfinite_,
             this->calibration_rejected_rs_);
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
