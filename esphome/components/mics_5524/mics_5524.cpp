#include "mics_5524.h"

#include <algorithm>
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mics_5524 {

ESPHOME_LOG_TAG(TAG, "mics_5524");

/// Added to the conversion model, so a value stored for one model is never loaded by the other.
static constexpr uint32_t REFERENCE_PREFERENCE_VERSION = 0x00000010;

/// Calibration samples of a request on a sensor without `calibration:`, the schema default.
static constexpr uint32_t DEFAULT_CALIBRATION_SAMPLES = 10;

static const LogString *reference_name(uint8_t conversion) {
  return conversion == micsmath::CONVERSION_MODEL_DFROBOT ? LOG_STR("air reference") : LOG_STR("R0");
}

static const LogString *conversion_name(uint8_t conversion) {
  return conversion == micsmath::CONVERSION_MODEL_DFROBOT ? LOG_STR("dfrobot") : LOG_STR("datasheet");
}

void MiCS5524GasSensor::publish_ratio(float ratio) {
  const float ppm = this->is_vendor_model()
                        ? micsmath::ppm_from_vendor_curve(ratio, this->curve_)
                        : micsmath::clamp_ppm(micsmath::ppm_from_power_law(this->a_, this->b_, ratio),
                                              this->curve_.min_ppm, this->curve_.max_ppm);
  ESP_LOGD(TAG, "'%s': ratio=%.4f -> %.1f ppm", this->get_name().c_str(), ratio, ppm);
  this->publish_state(ppm);
}

void MiCS5524Component::setup() {
  this->reference_pref_ = this->make_entity_preference<float>(REFERENCE_PREFERENCE_VERSION + this->conversion_);

  if (this->enable_pin_ != nullptr) {
    // EN of the breakout is active low; `inverted: true` makes this write switch the module on.
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(true);
  }

  if (this->has_reference_()) {
    ESP_LOGI(TAG, "using the %s from the configuration (%.4f) - the sensor is not calibrated at boot",
             LOG_STR_ARG(reference_name(this->conversion_)), this->reference_);
  } else if (this->persist_ && this->load_reference_()) {
    ESP_LOGI(TAG, "restored the %s from flash (%.4f)", LOG_STR_ARG(reference_name(this->conversion_)),
             this->reference_);
  }

  if (this->warmup_time_ > 0) {
    this->warming_up_ = true;
    this->stop_poller();
    this->set_timeout(this->warmup_time_, [this]() {
      this->warming_up_ = false;
      ESP_LOGI(TAG, "warm-up finished, readings are published from now on");
      this->start_poller();
      // A calibration waiting for the warm-up starts when one at boot would have.
      if (this->calibration_pending_) {
        const uint32_t delay =
            this->calibration_delay_ > this->warmup_time_ ? this->calibration_delay_ - this->warmup_time_ : 0;
        this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
      }
    });
    ESP_LOGI(TAG, "warm-up of %" PRIu32 " s - no readings published before that", this->warmup_time_ / 1000);
  }

  if (this->calibration_samples_ > 0) {
    // A fixed or restored reference wins.
    if (this->has_reference_()) {
      ESP_LOGI(TAG, "%s already known (%.4f) - automatic calibration skipped",
               LOG_STR_ARG(reference_name(this->conversion_)), this->reference_);
    } else {
      this->calibration_pending_ = true;
      if (!this->warming_up_)
        this->set_timeout(CALIBRATION_START_ID, this->calibration_delay_, [this]() { this->begin_calibration_(); });
      ESP_LOGI(TAG, "clean-air calibration of the %s in %" PRIu32 " s - the sensor must be in clean air",
               LOG_STR_ARG(reference_name(this->conversion_)),
               std::max(this->calibration_delay_, this->warmup_time_) / 1000);
    }
  } else if (!this->has_reference_()) {
    ESP_LOGW(TAG,
             "no %s available, the PPM values stay unknown\n  set it in the configuration or enable 'calibration:'",
             LOG_STR_ARG(reference_name(this->conversion_)));
  }

  // The loop only samples while a clean-air calibration runs; `update()` publishes.
  this->disable_loop();
}

void MiCS5524Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MiCS-5524:\n"
                "  Conversion: %s, VCC: %.2f V, AO multiplier: x%.3f, AO samples per reading: %u\n"
                "  Warm-up: %" PRIu32 " s, calibration: %s, sample spacing: %" PRIu32 " ms",
                LOG_STR_ARG(conversion_name(this->conversion_)), this->vcc_, this->voltage_multiplier_,
                static_cast<unsigned>(this->samples_), this->warmup_time_ / 1000,
                this->calibration_samples_ > 0 ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"),
                this->sample_interval_);
  LOG_UPDATE_INTERVAL(this);
  LOG_PIN("  Enable pin: ", this->enable_pin_);
  if (!this->is_vendor_model_()) {
    ESP_LOGCONFIG(TAG, "  Load resistor: %.1f kOhm", this->rl_);
  }
  if (this->has_reference_()) {
    ESP_LOGCONFIG(TAG, "  %s: %.4f", LOG_STR_ARG(reference_name(this->conversion_)), this->reference_);
  } else {
    ESP_LOGCONFIG(TAG, "  %s: not available yet", LOG_STR_ARG(reference_name(this->conversion_)));
  }
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Ratio", this->ratio_sensor_);
  LOG_SENSOR("  ", "RS", this->rs_sensor_);
#ifdef MICS_5524_GAS_COUNT
  for (auto *gas : this->gases_) {
    LOG_SENSOR("  ", "Gas", gas);
    ESP_LOGCONFIG(TAG, "    Range: %.1f - %.1f ppm", gas->get_min_ppm(), gas->get_max_ppm());
  }
#endif
}

float MiCS5524Component::sample_voltage_() {
  // Taken back to back; waiting between them would block the main loop.
  float sum = 0.0f;
  for (uint8_t i = 0; i < this->samples_; i++)
    sum += this->source_->sample();
  return (sum / static_cast<float>(this->samples_)) * this->voltage_multiplier_;
}

float MiCS5524Component::model_value_(float voltage) {
  float value;
  if (this->is_vendor_model_()) {
    // A disconnected input reads near 0 V, which would convert to a ratio near 1.0 and read as clean air.
    value = voltage > 0.05f * this->vcc_ ? micsmath::air_value_from_voltage(voltage, this->vcc_) : NAN;
  } else {
    value = micsmath::rs_from_voltage(voltage, this->vcc_, this->rl_);
  }
  if (!std::isnan(value))
    return value;
  // The status flag makes this log once per fault, not once per update.
  if (!this->status_has_warning()) {
    ESP_LOGW(TAG, "analog output reads %.4f V with vcc %.2f V\n  check the AO wiring, 'vcc:' and 'voltage_multiplier:'",
             voltage, this->vcc_);
  }
  this->status_set_warning(LOG_STR("analog output out of range"));
  return NAN;
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
  if (this->rs_sensor_ != nullptr && !this->is_vendor_model_())
    this->rs_sensor_->publish_state(rs);
}

void MiCS5524Component::update() {
  const float voltage = this->sample_voltage_();
  const float value = this->model_value_(voltage);
  float ratio = NAN;
  if (this->has_reference_()) {
    ratio = this->is_vendor_model_() ? micsmath::ratio_from_air_value(value, this->reference_)
                                     : micsmath::ratio_from_rs(value, this->reference_);
  }
  this->publish_diagnostics_(voltage, ratio, this->is_vendor_model_() ? NAN : value);

  // A failed calibration keeps its warning until the retry succeeds.
  if (!std::isnan(value) && !this->calibrating_ && !this->calibration_pending_)
    this->status_clear_warning();

  // Every gas starts unknown and the reference is never cleared, so there is nothing to republish.
  if (!this->has_reference_())
    return;

  ESP_LOGD(TAG, "V_AO=%.4f V, ratio=%.4f", voltage, ratio);
  this->publish_ratio_(ratio);
}

void MiCS5524Component::set_calibration(uint32_t delay, uint32_t samples, uint32_t sample_interval, bool persist) {
  this->calibration_delay_ = delay;
  this->calibration_samples_ = samples;
  this->sample_interval_ = sample_interval;
  this->persist_ = persist;
}

void MiCS5524Component::set_air_reference(float air_reference) { this->reference_ = air_reference; }

void MiCS5524Component::set_r0(float r0) { this->reference_ = r0; }

void MiCS5524Component::request_calibration() {
  if (this->calibrating_) {
    ESP_LOGW(TAG, "calibration already running");
    return;
  }
  if (this->calibration_pending_) {
    ESP_LOGW(TAG, "calibration already requested");
    return;
  }
  if (this->calibration_samples_ == 0)
    this->calibration_samples_ = DEFAULT_CALIBRATION_SAMPLES;
  this->calibration_pending_ = true;
  if (this->warming_up_) {
    // A request must never capture an unstable reading; the warm-up timeout starts it.
    ESP_LOGI(TAG, "calibration requested - waiting for the warm-up window to end");
    return;
  }
  this->set_timeout(CALIBRATION_START_ID, 0, [this]() { this->begin_calibration_(); });
  ESP_LOGI(TAG, "calibration requested - keep the sensor in clean air");
}

void MiCS5524Component::loop() {
  if (!this->calibrating_) {
    // Only a calibration samples here; the deferred start is a timer.
    this->disable_loop();
    return;
  }

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
  this->calibration_min_ = INFINITY;
  this->calibration_max_ = -INFINITY;
  this->enable_loop();

  ESP_LOGI(TAG, "calibrating the %s, %" PRIu32 " samples - keep the sensor in clean air",
           LOG_STR_ARG(reference_name(this->conversion_)), this->calibration_samples_);
}

void MiCS5524Component::process_calibration_() {
  this->calibration_attempts_++;

  // The samples are averaged in the units of the active model.
  const float reference = this->model_value_(this->sample_voltage_());
  if (!std::isnan(reference)) {
    this->calibration_min_ = std::min(this->calibration_min_, reference);
    this->calibration_max_ = std::max(this->calibration_max_, reference);
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
  // A majority of the attempts has to have produced a sample, two at least.
  const uint32_t required = std::max<uint32_t>((attempts + 1) / 2, 2);

  if (valid < required) {
    ESP_LOGW(TAG, "calibration failed, %" PRIu32 " of %" PRIu32 " samples were valid", valid, attempts);
    this->schedule_calibration_retry_();
    return;
  }

  const float reference = this->calibration_sum_ / static_cast<float>(valid);

  // A sensor still settling, or not in clean air, varies too much to be the reference.
  if (!micsmath::calibration_is_stable(this->calibration_min_, this->calibration_max_, reference)) {
    ESP_LOGW(TAG, "clean-air samples spread %.4f around %.4f (%.4f - %.4f)\n  the sensor was not settled in clean air",
             this->calibration_max_ - this->calibration_min_, reference, this->calibration_min_,
             this->calibration_max_);
    this->schedule_calibration_retry_();
    return;
  }

  this->reference_ = reference;
  this->status_clear_warning();
  if (this->persist_)
    this->save_reference_();

  ESP_LOGI(TAG,
           "%s = %.4f (%" PRIu32 "/%" PRIu32 " samples valid)%s\n"
           "  hard-code '%s: %.4f' to skip the calibration at boot",
           LOG_STR_ARG(reference_name(this->conversion_)), this->reference_, valid, attempts,
           this->persist_ ? LOG_STR_LITERAL(", stored in flash") : LOG_STR_LITERAL(""),
           this->is_vendor_model_() ? LOG_STR_LITERAL("air_reference") : LOG_STR_LITERAL("r0"), this->reference_);
}

void MiCS5524Component::schedule_calibration_retry_() {
  this->status_set_warning();
  this->calibration_pending_ = true;
  // A zero delay would log the same failure about five times per second.
  const uint32_t delay = std::max(this->calibration_delay_, CALIBRATION_RETRY_MIN_MS);
  this->set_timeout(CALIBRATION_START_ID, delay, [this]() { this->begin_calibration_(); });
  ESP_LOGW(TAG, "retrying the calibration in %" PRIu32 " s", delay / 1000);
}

void MiCS5524Component::save_reference_() {
  if (!this->reference_pref_.save(&this->reference_)) {
    ESP_LOGW(TAG, "could not store the %s in flash", LOG_STR_ARG(reference_name(this->conversion_)));
    return;
  }
  ESP_LOGD(TAG, "%s %.4f saved", LOG_STR_ARG(reference_name(this->conversion_)), this->reference_);
}

bool MiCS5524Component::load_reference_() {
  float stored = 0.0f;
  if (!this->reference_pref_.load(&stored))
    return false;
  if (!std::isfinite(stored) || stored <= 0.0f)
    return false;
  this->reference_ = stored;
  return true;
}

}  // namespace esphome::mics_5524
