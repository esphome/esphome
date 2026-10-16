#include "mics_5524_gas_sensor.h"

#include <algorithm>
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mics_5524_gas_sensor {

ESPHOME_LOG_TAG(TAG, "mics_5524_gas_sensor");

/// Name of the clean-air reference, which differs per conversion model.
static const char *reference_name(uint8_t conversion) {
  return conversion == micsmath::CONVERSION_MODEL_DFROBOT ? LOG_STR_LITERAL("air reference") : LOG_STR_LITERAL("R0");
}

/// Name of a conversion model for the log lines.
static const char *conversion_name(uint8_t conversion) {
  return conversion == micsmath::CONVERSION_MODEL_DFROBOT ? LOG_STR_LITERAL("dfrobot") : LOG_STR_LITERAL("datasheet");
}

void MiCS5524GasSensor::publish_ratio(float ratio) {
  // Both models share the pipeline up to the ratio; only the conversion differs.
  const float ppm = this->is_vendor_model()
                        ? micsmath::ppm_from_vendor_curve(ratio, this->curve_)
                        : micsmath::clamp_ppm(micsmath::ppm_from_power_law(this->a_, this->b_, ratio),
                                              this->curve_.min_ppm, this->curve_.max_ppm);
  ESP_LOGD(TAG, "'%s': ratio=%.4f -> %.1f ppm", this->gas_, ratio, ppm);
  this->publish_state(ppm);
}

void MiCS5524Component::setup() {
  if (this->enable_pin_ != nullptr) {
    // The module's EN pin only switches the analog front end on; the warm-up window
    // below is what waits for the readings to settle.
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(true);
  }

  if (this->has_reference_()) {
    ESP_LOGI(TAG, "using the %s from the configuration (%.4f) - the sensor is not calibrated at boot",
             reference_name(this->conversion_), this->reference_);
  }

  if (this->warmup_time_ > 0) {
    this->warmup_start_ = App.get_loop_component_start_time();
    ESP_LOGI(TAG, "warm-up of %" PRIu32 " s - no readings published before that", this->warmup_time_ / 1000);
  }

  if (this->calibration_enabled_) {
    // Nothing is stored across reboots, so every boot calibrates when it is configured
    // to; the reference in the configuration is the way to skip that.
    // The warm-up window starts here, so all of it still lies ahead of the calibration.
    const uint32_t delay = std::max(this->calibration_delay_, this->warmup_time_);
    this->calibration_pending_ = true;
    this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
    ESP_LOGI(TAG, "clean-air calibration of the %s in %" PRIu32 " s - the sensor must be in clean air",
             reference_name(this->conversion_), delay / 1000);
  } else if (!this->has_reference_()) {
    ESP_LOGW(TAG,
             "no %s available, set it in the configuration or enable 'calibration:' - the PPM values stay "
             "unknown until then",
             reference_name(this->conversion_));
  }

  // The loop only samples while a clean-air calibration runs; `update()` publishes.
  this->disable_loop();
}

void MiCS5524Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MiCS-5524:");
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Conversion: %s", conversion_name(this->conversion_));
#ifdef MICS_5524_GAS_COUNT
  for (auto *gas : this->gases_) {
    LOG_SENSOR("  ", "Gas", gas);
    ESP_LOGCONFIG(TAG, "    Range: %.1f - %.1f ppm", gas->get_min_ppm(), gas->get_max_ppm());
  }
#endif
  ESP_LOGCONFIG(TAG, "  VCC: %.2f V, AO multiplier: x%.3f", this->vcc_, this->voltage_multiplier_);
  if (!this->is_vendor_model_()) {
    ESP_LOGCONFIG(TAG, "  Load resistor: %.1f kOhm", this->rl_);
  }
  ESP_LOGCONFIG(TAG, "  AO samples per reading: %u", static_cast<unsigned>(this->samples_));
  if (this->has_reference_()) {
    ESP_LOGCONFIG(TAG, "  %s: %.4f", reference_name(this->conversion_), this->reference_);
  } else {
    ESP_LOGCONFIG(TAG, "  %s: not available yet", reference_name(this->conversion_));
  }
  ESP_LOGCONFIG(
      TAG, "  Warm-up: %" PRIu32 " s, calibration: %s, sample spacing: %" PRIu32 " ms", this->warmup_time_ / 1000,
      this->calibration_enabled_ ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"), this->sample_interval_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Ratio", this->ratio_sensor_);
  LOG_SENSOR("  ", "RS", this->rs_sensor_);
}

float MiCS5524Component::sample_voltage_() {
  // Taken back to back: the sampler averages internally as configured (`samples` /
  // `sampling_mode`), and waiting between the two would block the main loop.
  float sum = 0.0f;
  for (uint8_t i = 0; i < this->samples_; i++)
    sum += this->source_->sample();
  return (sum / static_cast<float>(this->samples_)) * this->voltage_multiplier_;
}

float MiCS5524Component::reference_for_voltage_(float voltage) const {
  // The clean-air reference of the active model: how far the output sits below the
  // supply (vendor), or the sensor resistance that implies (datasheet).
  return this->is_vendor_model_() ? micsmath::air_value_from_voltage(voltage, this->vcc_)
                                  : micsmath::rs_from_voltage(voltage, this->vcc_, this->rl_);
}

float MiCS5524Component::ratio_for_voltage_(float voltage) {
  // Without a reference every reading would be divided by zero, and there is nothing to
  // report until the calibration has run.
  if (!this->has_reference_())
    return NAN;
  if (this->is_vendor_model_())
    return micsmath::ratio_from_air_value(micsmath::air_value_from_voltage(voltage, this->vcc_), this->reference_);
  return micsmath::ratio_from_rs(micsmath::rs_from_voltage(voltage, this->vcc_, this->rl_), this->reference_);
}

void MiCS5524Component::publish_ratio_(float ratio) {
#ifdef MICS_5524_GAS_COUNT
  for (auto *gas : this->gases_) {
    gas->publish_ratio(ratio);
  }
#endif
}

void MiCS5524Component::publish_diagnostics_(float voltage, float ratio, float rs) {
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(voltage);
  if (this->ratio_sensor_ != nullptr)
    this->ratio_sensor_->publish_state(ratio);
  // RS only exists in the datasheet model, so the vendor model leaves its sensor alone
  // rather than publishing a value nothing computed.
  if (this->rs_sensor_ != nullptr && !this->is_vendor_model_())
    this->rs_sensor_->publish_state(rs);
}

bool MiCS5524Component::in_warmup_() {
  if (this->warmup_ended_)
    return false;
  if (App.get_loop_component_start_time() - this->warmup_start_ < this->warmup_time_)
    return true;
  this->warmup_ended_ = true;
  ESP_LOGI(TAG, "warm-up finished, readings are published from now on");
  return false;
}

void MiCS5524Component::update() {
  const float voltage = this->sample_voltage_();

  // The warm-up window is part of the sensor settling, so nothing is published before
  // it has passed.
  if (this->in_warmup_())
    return;

  const float rs = this->is_vendor_model_() ? NAN : micsmath::rs_from_voltage(voltage, this->vcc_, this->rl_);
  const float ratio = this->ratio_for_voltage_(voltage);
  this->publish_diagnostics_(voltage, ratio, rs);

  if (this->has_reference_()) {
    this->warned_no_reference_ = false;
  } else if (!this->warned_no_reference_) {
    this->warned_no_reference_ = true;
    ESP_LOGW(TAG, "no clean-air reference yet - every gas stays 'unknown' until the %s is known",
             reference_name(this->conversion_));
  }

  if (std::isfinite(ratio)) {
    this->warned_voltage_ = false;
  } else if (!this->warned_voltage_ && this->has_reference_()) {
    this->warned_voltage_ = true;
    ESP_LOGW(TAG,
             "analog output reads %.4f V, which cannot be converted - check 'vcc:', 'voltage_multiplier:' and "
             "the wiring",
             voltage);
  }

  this->publish_ratio_(ratio);
}

void MiCS5524Component::loop() {
  if (!this->calibrating_)
    return;
  // Only the calibration samples run on the loop; `update()` publishes on its own
  // interval, so this gate is what spaces the clean-air samples.
  if (App.get_loop_component_start_time() - this->calibration_last_sample_ < this->sample_interval_)
    return;
  this->calibration_last_sample_ = App.get_loop_component_start_time();
  this->process_calibration_();
}

void MiCS5524Component::set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval) {
  this->calibration_enabled_ = true;
  this->calibration_delay_ = delay;
  this->calibration_samples_ = samples;
  this->sample_interval_ = sample_interval;
}

void MiCS5524Component::set_air_reference(float air_reference) { this->reference_ = air_reference; }

void MiCS5524Component::set_r0(float r0) { this->reference_ = r0; }

void MiCS5524Component::begin_calibration_() {
  this->calibration_pending_ = false;
  this->calibrating_ = true;
  this->calibration_last_sample_ = App.get_loop_component_start_time();
  this->calibration_count_ = 0;
  this->calibration_attempts_ = 0;
  this->calibration_sum_ = 0.0f;
  this->calibration_min_ = 0.0f;
  this->calibration_max_ = 0.0f;
  this->enable_loop();

  ESP_LOGI(TAG, "calibrating the %s, %" PRIu32 " samples - keep the sensor in clean air",
           reference_name(this->conversion_), this->calibration_samples_);
}

void MiCS5524Component::process_calibration_() {
  this->calibration_attempts_++;

  const float voltage = this->sample_voltage_();
  // Both models store their clean-air value in the same field, so the samples are
  // averaged in the units of the active one.
  const float reference = this->reference_for_voltage_(voltage);
  if (std::isfinite(reference) && reference > 0.0f) {
    if (this->calibration_count_ == 0) {
      this->calibration_min_ = reference;
      this->calibration_max_ = reference;
    } else {
      this->calibration_min_ = std::min(this->calibration_min_, reference);
      this->calibration_max_ = std::max(this->calibration_max_, reference);
    }
    this->calibration_sum_ += reference;
    this->calibration_count_++;
  }

  if (this->calibration_attempts_ >= this->calibration_samples_)
    this->finish_calibration_();
}

void MiCS5524Component::finish_calibration_() {
  this->calibrating_ = false;
  this->disable_loop();

  const uint32_t attempts = this->calibration_attempts_;
  const uint32_t valid = this->calibration_count_;
  // Every reading uses this reference, so a lucky minority of samples must not define
  // it: a majority of the attempts has to have produced one, two samples at least.
  const uint32_t required = std::max<uint32_t>((attempts + 1) / 2, 2);

  if (valid < required) {
    ESP_LOGW(TAG, "calibration failed, %" PRIu32 " of %" PRIu32 " samples were valid", valid, attempts);
    this->schedule_calibration_retry_();
    return;
  }

  // Every counted sample is finite and positive (`process_calibration_()`), so their
  // average is usable without another check.
  const float reference = this->calibration_sum_ / static_cast<float>(valid);

  // A sensor that is still settling, or one that is not in clean air, varies too much
  // for its average to be the reference of every later reading.
  if (!micsmath::calibration_is_stable(this->calibration_min_, this->calibration_max_, reference)) {
    ESP_LOGW(TAG, "clean-air samples spread %.4f around %.4f (%.4f - %.4f) - the sensor was not settled in clean air",
             this->calibration_max_ - this->calibration_min_, reference, this->calibration_min_,
             this->calibration_max_);
    this->schedule_calibration_retry_();
    return;
  }

  this->reference_ = reference;
  this->status_clear_warning();

  ESP_LOGI(TAG, "%s = %.4f (%" PRIu32 "/%" PRIu32 " samples valid)", reference_name(this->conversion_),
           this->reference_, valid, attempts);
  if (this->is_vendor_model_()) {
    ESP_LOGI(TAG, "hard-code 'air_reference: %.4f' in the YAML to skip the calibration at boot", this->reference_);
  } else {
    ESP_LOGI(TAG, "hard-code 'r0: %.4f' in the YAML to skip the calibration at boot", this->reference_);
  }
}

void MiCS5524Component::schedule_calibration_retry_() {
  this->status_set_warning();
  this->calibration_pending_ = true;
  // A delay of zero restarts the run straight away and logs the same failure again about
  // five times per second, so anything below the floor is raised to it.
  const uint32_t delay = std::max(this->calibration_delay_, CALIBRATION_RETRY_MIN_MS);
  this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
  ESP_LOGW(TAG, "retrying the calibration in %" PRIu32 " s", delay / 1000);
}

}  // namespace esphome::mics_5524_gas_sensor
