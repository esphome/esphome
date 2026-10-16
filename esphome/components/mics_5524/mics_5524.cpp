#include "mics_5524.h"

#include <algorithm>
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mics_5524 {

ESPHOME_LOG_TAG(TAG, "mics_5524");

void MiCS5524GasSensor::publish_ratio(float ratio) {
  const float ppm = micsmath::ppm_from_vendor_curve(ratio, this->curve_);
  ESP_LOGD(TAG, "'%s': ratio=%.4f -> %.1f ppm", this->gas_, ratio, ppm);
  this->publish_state(ppm);
}

void MiCS5524Component::setup() {
  if (this->has_reference_()) {
    ESP_LOGI(TAG, "using the air reference from the configuration (%.4f V)", this->reference_);
  }

  if (this->warmup_time_ > 0) {
    this->warmup_start_ = App.get_loop_component_start_time();
    ESP_LOGI(TAG, "warm-up of %" PRIu32 " s - no readings published before that", this->warmup_time_ / 1000);
  }

  if (this->calibration_enabled_) {
    // Nothing is stored across reboots, so every boot calibrates when it is
    // configured to; 'air_reference:' is the way to skip that.
    // The warm-up window starts here, so all of it still lies ahead of the calibration.
    const uint32_t delay = std::max(this->calibration_delay_, this->warmup_time_);
    this->calibration_pending_ = true;
    this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
    ESP_LOGI(TAG, "air reference calibration in %" PRIu32 " s - the sensor must be in clean air", delay / 1000);
  } else if (!this->has_reference_()) {
    ESP_LOGW(TAG, "no air reference available, set it in the configuration or enable 'calibration:' - the PPM values "
                  "stay unknown until then");
  }

  // The loop only samples while a clean-air calibration runs; `update()` publishes.
  this->disable_loop();
}

void MiCS5524Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MiCS-5524:");
  LOG_UPDATE_INTERVAL(this);
#ifdef MICS_5524_GAS_COUNT
  for (auto *gas : this->gases_) {
    LOG_SENSOR("  ", "Gas", gas);
    ESP_LOGCONFIG(TAG, "    Range: %.1f - %.1f ppm", gas->get_min_ppm(), gas->get_max_ppm());
  }
#endif
  ESP_LOGCONFIG(TAG, "  VCC: %.2f V, AO multiplier: x%.3f", this->vcc_, this->voltage_multiplier_);
  ESP_LOGCONFIG(TAG, "  AO samples per reading: %u", static_cast<unsigned>(this->samples_));
  if (this->has_reference_()) {
    ESP_LOGCONFIG(TAG, "  Air reference (vcc - V_AO): %.4f V", this->reference_);
  } else {
    ESP_LOGCONFIG(TAG, "  Air reference: not available yet");
  }
  ESP_LOGCONFIG(
      TAG, "  Warm-up: %" PRIu32 " s, calibration: %s, sample spacing: %" PRIu32 " ms", this->warmup_time_ / 1000,
      this->calibration_enabled_ ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"), this->sample_interval_);
}

float MiCS5524Component::sample_voltage_() {
  // Taken back to back: the sampler averages internally as configured (`samples` /
  // `sampling_mode`), and waiting between the two would block the main loop.
  float sum = 0.0f;
  for (uint8_t i = 0; i < this->samples_; i++)
    sum += this->source_->sample();
  return (sum / static_cast<float>(this->samples_)) * this->voltage_multiplier_;
}

void MiCS5524Component::publish_ratio_(float ratio) {
#ifdef MICS_5524_GAS_COUNT
  for (auto *gas : this->gases_) {
    gas->publish_ratio(ratio);
  }
#endif
}

bool MiCS5524Component::in_warmup_() {
  if (this->warmup_ended_)
    return false;
  if (App.get_loop_component_start_time() - this->warmup_start_ < this->warmup_time_)
    return true;
  // A window that has passed is over: latching it keeps the subtraction above from
  // reopening it once it overflows, ~49.7 days after the start.
  this->warmup_ended_ = true;
  return false;
}

void MiCS5524Component::update() {
  if (this->calibrating_) {
    // The reference is about to change: never keep a value that was computed with
    // the previous one.
    this->publish_ratio_(NAN);
    ESP_LOGD(TAG, "update skipped, a calibration is running - the sensor must be in clean air");
    return;
  }

  if (this->calibration_pending_) {
    this->publish_ratio_(NAN);
    return;
  }

  if (this->in_warmup_()) {
    // Published on every pass, so a value computed before a restart never survives the
    // warm-up window.  `setup()` logged the window, so there is nothing to repeat here.
    this->publish_ratio_(NAN);
    return;
  }

  if (!this->has_reference_()) {
    if (!this->warned_no_reference_) {
      this->warned_no_reference_ = true;
      ESP_LOGW(TAG, "air reference unknown - publish it in the configuration or enable 'calibration:'");
    }
    this->publish_ratio_(NAN);
    return;
  }

  const float voltage = this->sample_voltage_();
  if (!std::isfinite(voltage) || voltage <= 0.0f) {
    if (!this->warned_voltage_) {
      this->warned_voltage_ = true;
      ESP_LOGW(TAG, "analog output reads %.4f V - open circuit, missing supply or bad divider? check the AO wiring",
               voltage);
    }
    // Nothing usable was read, so leave the gases unknown instead of publishing a
    // value that looks like a measurement.
    this->publish_ratio_(NAN);
    return;
  }

  const float air_value = micsmath::air_value_from_voltage(voltage, this->vcc_);
  if (!std::isfinite(air_value)) {
    // Only a `vcc:` below what the sampler reports can do this, and reading it as the
    // saturated end of the range would turn a configuration error into what looks like
    // a real measurement.
    if (!this->warned_voltage_) {
      this->warned_voltage_ = true;
      ESP_LOGW(TAG,
               "analog output reads %.4f V, at or above the configured vcc (%.2f V) - check 'vcc:' and "
               "'voltage_multiplier:'",
               voltage, this->vcc_);
    }
    this->publish_ratio_(NAN);
    return;
  }
  this->warned_voltage_ = false;

  const float ratio = micsmath::ratio_from_air_value(air_value, this->reference_);

  ESP_LOGD(TAG, "V_AO=%.4f V, x=%.4f, ratio=%.4f", voltage, air_value, ratio);

  this->publish_ratio_(ratio);
}

void MiCS5524Component::set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval) {
  this->calibration_enabled_ = true;
  this->calibration_delay_ = delay;
  this->calibration_samples_ = samples;
  this->sample_interval_ = sample_interval;
}

void MiCS5524Component::loop() {
  // The loop runs only while a calibration samples; the deferred start is a timer.
  if (!this->calibrating_)
    return;

  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->calibration_last_sample_ < this->sample_interval_)
    return;

  this->calibration_last_sample_ = now;
  this->process_calibration_();
}

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

  ESP_LOGI(TAG, "calibrating the air reference, %" PRIu32 " samples - keep the sensor in clean air",
           this->calibration_samples_);
}

void MiCS5524Component::process_calibration_() {
  this->calibration_attempts_++;

  const float voltage = this->sample_voltage_();
  if (std::isfinite(voltage) && voltage > 0.0f) {
    const float reference = micsmath::air_value_from_voltage(voltage, this->vcc_);
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
    ESP_LOGW(
        TAG, "clean-air samples spread %.4f V around %.4f V (%.4f - %.4f V) - the sensor was not settled in clean air",
        this->calibration_max_ - this->calibration_min_, reference, this->calibration_min_, this->calibration_max_);
    this->schedule_calibration_retry_();
    return;
  }

  this->reference_ = reference;
  this->status_clear_warning();

  ESP_LOGI(TAG, "air reference = %.4f (%" PRIu32 "/%" PRIu32 " samples valid)", this->reference_, valid, attempts);
  ESP_LOGI(TAG, "hard-code 'air_reference: %.4f' in the YAML to skip the calibration at boot", this->reference_);
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

}  // namespace esphome::mics_5524
