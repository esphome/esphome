#include "battery_gauge_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cmath>
#include <algorithm>

namespace esphome::battery_gauge {

static const char *const TAG = "battery_gauge.sensor";

void BatteryGaugeSensor::on_current_(float value) {
  if (!std::isfinite(value))
    return;  // ignore invalid values
  auto current = value;
  if (std::isfinite(this->last_current_)) {
    current += this->last_current_;
    current /= 2.0f;
  }
  this->last_current_ = value;
  // EMA filter
  if (!std::isfinite(this->filtered_current_)) {
    this->filtered_current_ = value;
  } else {
    this->filtered_current_ = value / 10.0f + this->filtered_current_ * 0.9f;
  }
  auto now = millis();
  auto previous = this->last_time_;
  this->last_time_ = now;
  if (previous == 0)
    return;
  // apply efficiency only to charge currents
  if (current > 0)
    current *= this->efficiency_;
  float interval = (now - previous) / 1000.0f / 3600.0f;
  auto delta = current * interval;
  ESP_LOGV(TAG, "current: %f, interval: %f, delta: %f, charge state: %f", current, interval, delta,
           this->charge_state_);
  this->publish_(this->charge_state_ + delta);
}

// Publish new charge state, in Ah
void BatteryGaugeSensor::publish_(float new_state) {
  this->charge_state_ = std::max(0.0f, std::min(new_state, this->capacity_));
  auto percentage = this->charge_state_ / this->capacity_ * 100.0f;
  this->publish_state(percentage);
  unsigned new_percentage = std::round(percentage * 10.0);
  if (new_percentage != this->charge_percentage_) {
    this->charge_percentage_ = new_percentage;
    ESP_LOGV(TAG, "Saving charge percentage: %u", this->charge_percentage_);
    this->saved_percentage_.save(&this->charge_percentage_);
  }
}
/**
 * Using voltage thresholds to adjust the charge state. This resynchronises at full charge
 */
void BatteryGaugeSensor::on_voltage_(float value) {
  // don't use voltage to adjust when under significant charge or discharge
  if (!std::isfinite(value))
    return;
  if (std::abs(this->last_current_) > this->capacity_ / 20)
    return;
  if (!std::isfinite(this->filtered_voltage_)) {
    this->filtered_voltage_ = value;
    return;
  }
  // smooth the voltage with an EMA filter
  this->filtered_voltage_ = value / 10.0f + this->filtered_voltage_ * 0.9f;
  if (this->filtered_voltage_ >= this->max_charge_voltage_ && this->filtered_current_ < this->capacity_ * .02) {
    this->publish_(this->capacity_);
    ESP_LOGD(TAG, "Charging: Voltage %f, state set to 100%%", this->filtered_voltage_);
  }
}

void BatteryGaugeSensor::setup() {
  this->saved_percentage_ = this->make_entity_preference<unsigned>();
  this->current_source_->add_on_state_callback([this](float value) { this->on_current_(value); });
  this->voltage_source_->add_on_state_callback([this](float value) { this->on_voltage_(value); });
  if (!this->saved_percentage_.load(&this->charge_percentage_)) {
    ESP_LOGD(TAG, "Setting initial charge state to %f", this->initial_state_);
    this->charge_percentage_ = this->initial_state_ * 1000.0f;
    this->saved_percentage_.save(&this->charge_percentage_);
  }
  this->charge_state_ = this->charge_percentage_ / 1000.0f * this->capacity_;
}

void BatteryGaugeSensor::dump_config() {
  LOG_SENSOR("", "Battery Gauge", this);
  if (this->capacity_ < 10.0f) {
    ESP_LOGCONFIG(TAG, "  Capacity: %.0fmAh", this->capacity_ * 1000.0);
  } else {
    ESP_LOGCONFIG(TAG, "  Capacity: %.1fAh", this->capacity_);
  }
  ESP_LOGCONFIG(TAG, "  Max charge voltage: %.1f", this->max_charge_voltage_);
  unsigned saved_charge;
  if (this->saved_percentage_.load(&saved_charge)) {
    ESP_LOGCONFIG(TAG, "  Saved charge percentage: %.1f", saved_charge / 10.0f);
  }
}
}  // namespace esphome::battery_gauge
