#include "mics_5524_gas_sensor.h"

#include <algorithm>
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mics_5524_gas_sensor {

ESPHOME_LOG_TAG(TAG, "mics_5524_gas_sensor");

/// Name of the clean-air reference in the log lines.
static const char *reference_name() { return "air reference"; }

void MiCS5524GasSensor::setup() {
  if (this->reference_configured_) {
    ESP_LOGI(TAG, "'%s': using the air reference from the configuration (%.4f)", this->gas_.c_str(), this->reference_);
  }

  if (this->warmup_time_ > 0) {
    this->warmup_start_ = App.get_loop_component_start_time();
    ESP_LOGI(TAG, "'%s': warm-up of %" PRIu32 " s - no readings published before that", this->gas_.c_str(),
             this->warmup_time_ / 1000);
  }

  if (this->calibration_enabled_) {
    // Nothing is stored across reboots, so every boot calibrates when it is
    // configured to; 'air_reference:' is the way to skip that.
    const uint32_t delay = std::max(this->calibration_delay_, this->warmup_remaining_());
    this->calibration_pending_ = true;
    this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
    ESP_LOGI(TAG, "'%s': air reference calibration in %" PRIu32 " s - the sensor must be in clean air",
             this->gas_.c_str(), delay / 1000);
  } else if (!this->has_reference()) {
    ESP_LOGW(TAG,
             "'%s': no air reference available, set it in the configuration or enable 'calibration:' - the PPM "
             "value stays unknown until then",
             this->gas_.c_str());
  }

  // The loop only samples while a clean-air calibration runs; `update()` publishes.
  this->disable_loop();
}

void MiCS5524GasSensor::log_config_() {
  ESP_LOGCONFIG(TAG, "  Gas: %s", this->gas_.c_str());
  ESP_LOGCONFIG(TAG, "  Vendor curve: threshold %.4f, gain %.8f, range %.1f - %.1f ppm", this->threshold_, this->gain_,
                this->vendor_min_ppm_, this->vendor_max_ppm_);
  ESP_LOGCONFIG(TAG, "  VCC: %.2f V, AO multiplier: x%.3f", this->vcc_, this->voltage_multiplier_);
  ESP_LOGCONFIG(TAG, "  Range: %.1f - %.1f ppm, AO samples per reading: %u, calibration spacing: %" PRIu32 " ms",
                this->min_ppm_, this->max_ppm_, static_cast<unsigned>(this->samples_), this->sample_interval_);
  if (this->has_reference()) {
    ESP_LOGCONFIG(TAG, "  %s: %.4f", reference_name(), this->reference_);
  } else {
    ESP_LOGCONFIG(TAG, "  %s: not available yet", reference_name());
  }
  ESP_LOGCONFIG(TAG, "  Warm-up: %" PRIu32 " s, auto calibration: %s", this->warmup_time_ / 1000,
                this->calibration_enabled_ ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"));
}

void MiCS5524GasSensor::dump_config() {
  LOG_SENSOR("", "MiCS-5524 gas sensor", this);
  LOG_UPDATE_INTERVAL(this);
  this->log_config_();
}

float MiCS5524GasSensor::sample_voltage_() {
  if (this->source_ == nullptr)
    return NAN;

  // Taken back to back: the sampler averages internally as configured (`samples` /
  // `sampling_mode`), and waiting between the two would block the main loop.
  float sum = 0.0f;
  for (uint8_t i = 0; i < this->samples_; i++)
    sum += this->source_->sample();
  return (sum / static_cast<float>(this->samples_)) * this->voltage_multiplier_;
}

micsmath::VendorCurve MiCS5524GasSensor::vendor_curve_() const {
  return micsmath::VendorCurve{this->threshold_, this->gain_, this->vendor_min_ppm_, this->vendor_max_ppm_};
}

float MiCS5524GasSensor::current_ratio_() {
  this->air_value_ = micsmath::air_value_from_voltage(this->sensor_voltage_, this->vcc_);
  return micsmath::ratio_from_air_value(this->air_value_, this->reference_);
}

float MiCS5524GasSensor::read_ppm_() {
  const float ppm = micsmath::ppm_from_vendor_curve(this->ratio_, this->vendor_curve_());
  return micsmath::clamp_ppm(ppm, this->min_ppm_, this->max_ppm_);
}

void MiCS5524GasSensor::update() {
  if (this->is_failed())
    return;

  if (this->calibrating_) {
    // The reference is about to change: never keep a value that was computed with
    // the previous one.
    this->publish_state(NAN);
    ESP_LOGD(TAG, "'%s': update skipped, a calibration is running - the sensor must be in clean air",
             this->gas_.c_str());
    return;
  }

  if (this->calibration_pending_) {
    this->publish_state(NAN);
    return;
  }

  const uint32_t warmup_left = this->warmup_remaining_();
  if (warmup_left > 0) {
    if (!this->warmup_notified_) {
      this->warmup_notified_ = true;
      this->publish_state(NAN);
      ESP_LOGI(TAG, "'%s': warming up, %" PRIu32 " s to go", this->gas_.c_str(), warmup_left / 1000);
    }
    return;
  }

  if (!this->has_reference()) {
    if (!this->warned_no_reference_) {
      this->warned_no_reference_ = true;
      ESP_LOGW(TAG, "'%s': %s unknown - publish it in the configuration or enable 'calibration:'", this->gas_.c_str(),
               reference_name());
    }
    this->publish_state(NAN);
    return;
  }

  this->sensor_voltage_ = this->sample_voltage_();
  if (!std::isfinite(this->sensor_voltage_) || this->sensor_voltage_ <= 0.01f) {
    if (!this->warned_voltage_) {
      this->warned_voltage_ = true;
      ESP_LOGW(TAG,
               "'%s': analog output reads %.4f V - open circuit, missing supply or bad divider? check the AO wiring",
               this->gas_.c_str(), this->sensor_voltage_);
    }
    // Nothing usable was read, so leave the internal values unknown instead of
    // reporting a ratio that looks like a measurement.
    this->air_value_ = NAN;
    this->ratio_ = NAN;
    this->publish_state(NAN);
    return;
  }
  this->warned_voltage_ = false;

  this->ratio_ = this->current_ratio_();
  const float ppm = this->read_ppm_();

  ESP_LOGD(TAG, "'%s': V_AO=%.4f V, x=%.4f, ratio=%.4f -> %.1f ppm", this->gas_.c_str(), this->sensor_voltage_,
           this->air_value_, this->ratio_, ppm);

  this->publish_state(ppm);
}

void MiCS5524GasSensor::set_calibration(bool enabled, uint32_t delay, uint32_t samples) {
  this->calibration_enabled_ = enabled;
  this->calibration_delay_ = delay;
  this->calibration_samples_ = samples;
}

void MiCS5524GasSensor::set_air_reference(float air_reference) {
  this->reference_ = air_reference;
  this->reference_configured_ = true;
}

uint32_t MiCS5524GasSensor::warmup_remaining_() const {
  const uint32_t elapsed = App.get_loop_component_start_time() - this->warmup_start_;
  return elapsed < this->warmup_time_ ? this->warmup_time_ - elapsed : 0;
}

void MiCS5524GasSensor::request_calibration() {
  if (this->calibrating_) {
    ESP_LOGW(TAG, "'%s': calibration already running", this->gas_.c_str());
    return;
  }
  if (this->calibration_samples_ == 0) {
    ESP_LOGE(TAG, "'%s': cannot calibrate without samples", this->gas_.c_str());
    return;
  }
  if (this->calibration_pending_) {
    ESP_LOGW(TAG, "'%s': calibration already scheduled - restarting the wait", this->gas_.c_str());
  }

  // A request (button, on_boot lambda) must never capture an unstable reading, so
  // it is deferred until the warm-up / burn-in window of this sensor has ended.
  this->cancel_timeout(CALIBRATION_START_ID);
  const uint32_t warmup_left = this->warmup_remaining_();
  if (warmup_left > 0) {
    this->calibration_pending_ = true;
    this->set_timeout(CALIBRATION_START_ID, warmup_left, [this]() { this->begin_calibration_(); });
    ESP_LOGI(TAG, "'%s': calibration requested - waiting for the warm-up window to end (%" PRIu32 " s)",
             this->gas_.c_str(), warmup_left / 1000);
    return;
  }

  ESP_LOGI(TAG, "'%s': calibration requested - keep the sensor in clean air", this->gas_.c_str());
  this->begin_calibration_();
}

void MiCS5524GasSensor::loop() {
  if (this->is_failed())
    return;

  // The loop runs only while a calibration samples; the deferred start is a timer.
  if (!this->calibrating_)
    return;

  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->calibration_last_sample_ < this->sample_interval_)
    return;

  this->calibration_last_sample_ = now;
  this->process_calibration_();
}

void MiCS5524GasSensor::begin_calibration_() {
  this->calibration_pending_ = false;
  this->calibrating_ = true;
  this->calibration_last_sample_ = App.get_loop_component_start_time();
  this->calibration_count_ = 0;
  this->calibration_attempts_ = 0;
  this->calibration_sum_ = 0.0f;
  this->enable_loop();

  ESP_LOGI(TAG, "'%s': calibrating the air reference, %" PRIu32 " samples - keep the sensor in clean air",
           this->gas_.c_str(), this->calibration_samples_);
}

void MiCS5524GasSensor::process_calibration_() {
  this->calibration_attempts_++;

  const float voltage = this->sample_voltage_();
  if (std::isfinite(voltage) && voltage > 0.01f) {
    this->sensor_voltage_ = voltage;
    const float reference = micsmath::air_value_from_voltage(voltage, this->vcc_);
    if (std::isfinite(reference) && reference > 0.0f) {
      this->calibration_sum_ += reference;
      this->calibration_count_++;
    }
  }

  if (this->calibration_attempts_ >= this->calibration_samples_)
    this->finish_calibration_();
}

void MiCS5524GasSensor::finish_calibration_() {
  this->calibrating_ = false;
  this->disable_loop();
  const uint32_t attempts = this->calibration_attempts_;
  const uint32_t valid = this->calibration_count_;

  if (valid == 0) {
    ESP_LOGE(TAG,
             "'%s': calibration failed, none of the %" PRIu32
             " samples produced a valid reading - check the AO wiring and the supply",
             this->gas_.c_str(), attempts);
    return;
  }

  const float reference = this->calibration_sum_ / static_cast<float>(valid);
  if (!std::isfinite(reference) || reference <= 0.0f) {
    ESP_LOGE(TAG, "'%s': calibration produced an invalid %s (%.6f)", this->gas_.c_str(), reference_name(), reference);
    return;
  }

  this->reference_ = reference;
  this->reference_configured_ = true;

  ESP_LOGI(TAG, "'%s': %s = %.4f (%" PRIu32 "/%" PRIu32 " samples valid)", this->gas_.c_str(), reference_name(),
           this->reference_, valid, attempts);
  ESP_LOGI(TAG, "'%s': hard-code 'air_reference: %.4f' in the YAML to skip the calibration at boot", this->gas_.c_str(),
           this->reference_);
}

}  // namespace esphome::mics_5524_gas_sensor
