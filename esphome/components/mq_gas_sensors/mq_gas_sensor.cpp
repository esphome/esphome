#include "mq_gas_sensor.h"

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::mq_gas_sensors {

ESPHOME_LOG_TAG(TAG, "mq_gas_sensors");

/// Indexed by `RegressionMethod - 1`, so the order matches the enum values.
PROGMEM_STRING_TABLE(RegressionMethodNames, "exponential", "linear", "inverse");

/// Human readable name of a `RegressionMethod` value.
static const LogString *regression_method_name(RegressionMethod method) {
  return RegressionMethodNames::get_log_str(static_cast<uint8_t>(method) - 1, 0);
}

void MQGasSensor::dump_config() {
  LOG_SENSOR("", "MQ gas sensor", this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  Type: %s, target gas: %s\n"
                "  Curve: %s (a=%.6g, b=%.6g)\n"
                "  VCC: %.2f V, RL: %.2f kOhm, AO multiplier: x%.3f, range: %.1f - %.1f ppm\n"
                "  RS/R0 in clean air: %.2f, R0: %.4f kOhm\n"
                "  AO samples per reading: %u",
                this->type_.c_str(), this->gas_.c_str(), LOG_STR_ARG(regression_method_name(this->regression_method_)),
                this->a_, this->b_, this->vcc_, this->rl_, this->voltage_multiplier_, this->min_ppm_, this->max_ppm_,
                this->ratio_in_clean_air_, this->r0_, static_cast<unsigned>(this->samples_));
}

float MQGasSensor::sample_voltage_() {
  float sum = 0.0f;
  this->sample_count_ = 0;
  for (uint8_t i = 0; i < this->samples_; i++) {
    const float sample = this->source_->sample();
    // A conversion the sampler could not complete is not a measurement: drop it
    // instead of letting one NaN poison the average of the whole window.
    if (!std::isfinite(sample))
      continue;
    sum += sample;
    this->sample_count_++;
  }
  if (this->sample_count_ == 0)
    return NAN;
  return (sum / static_cast<float>(this->sample_count_)) * this->voltage_multiplier_;
}

float MQGasSensor::current_rs_(float voltage) const { return mqmath::rs_from_voltage(voltage, this->vcc_, this->rl_); }

float MQGasSensor::current_ratio_(float rs) const { return mqmath::ratio_from_rs(rs, this->r0_); }

float MQGasSensor::read_ppm_(float ratio) const {
  const float ppm = mqmath::ppm_from_ratio(this->a_, this->b_, ratio, this->regression_method_);
  if (!std::isfinite(ppm))
    return NAN;
  return mqmath::clamp_ppm(ppm, this->min_ppm_, this->max_ppm_);
}

void MQGasSensor::update() {
  if (this->is_failed())
    return;

  const float voltage = this->sample_voltage_();
  if (!std::isfinite(voltage)) {
    // The sampler itself produced no conversion: not the same fault as a wiring
    // that reads 0 V, and pointing at the AO wiring would hide a failed ADC.
    if (!this->warned_sampler_failure_) {
      this->warned_sampler_failure_ = true;
      ESP_LOGW(TAG, "'%s %s': the voltage sampler returned no reading - check the sampler (I2C bus, ADC)",
               this->type_.c_str(), this->gas_.c_str());
    }
    this->status_set_warning(LOG_STR("the voltage sampler returned no reading"));
    this->publish_state(NAN);
    return;
  }
  this->warned_sampler_failure_ = false;
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

  // At the supply, RS has collapsed to 0. The curve decides what that means: one
  // that grows as the ratio falls reports its top, a real high concentration that
  // must not look like a fault; one that falls there has no concentration to
  // report, and the maximum would contradict the configured curve.
  const float rs = saturated ? 0.0f : this->current_rs_(voltage);
  const float ratio = saturated ? 0.0f : this->current_ratio_(rs);

  float ppm = this->max_ppm_;
  if (saturated) {
    if (!(mqmath::ppm_at_zero(this->a_, this->b_, this->regression_method_) >= this->max_ppm_)) {
      if (!this->warned_curve_failure_) {
        this->warned_curve_failure_ = true;
        ESP_LOGW(TAG, "'%s %s': V=%.4f V is at the supply, but the configured curve does not reach its maximum there",
                 this->type_.c_str(), this->gas_.c_str(), voltage);
      }
      this->status_set_warning(LOG_STR("the input is saturated, but the curve has no top there"));
      this->publish_state(NAN);
      return;
    }
  } else {
    ppm = this->read_ppm_(ratio);
  }

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

  ESP_LOGV(TAG, "'%s %s': V=%.4f V, RS=%.4f kOhm, ratio=%.4f -> %.1f ppm", this->type_.c_str(), this->gas_.c_str(),
           voltage, rs, ratio, ppm);
  this->publish_state(ppm);
}

}  // namespace esphome::mq_gas_sensors
