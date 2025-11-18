/**
 * @file axp2101_number.cpp
 * @brief Implementation of AXP2101 number component
 */
#include "axp2101_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace axp2101 {

static const char *const TAG = "axp2101.number";

static const char *power_rail_to_string(PowerRail rail) {
  switch (rail) {
    case DCDC1:
      return "DCDC1";
    case DCDC2:
      return "DCDC2";
    case DCDC3:
      return "DCDC3";
    case DCDC4:
      return "DCDC4";
    case DCDC5:
      return "DCDC5";
    case ALDO1:
      return "ALDO1";
    case ALDO2:
      return "ALDO2";
    case ALDO3:
      return "ALDO3";
    case ALDO4:
      return "ALDO4";
    case BLDO1:
      return "BLDO1";
    case BLDO2:
      return "BLDO2";
    case CPUSLDO:
      return "CPUSLDO";
    case DLDO1:
      return "DLDO1";
    case DLDO2:
      return "DLDO2";
    default:
      return "UNKNOWN";
  }
}

void AXP2101Number::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set!");
    this->mark_failed();
    return;
  }

  // Read current voltage and publish initial state
  uint16_t current_mv = this->parent_->get_rail_voltage(this->rail_);
  if (current_mv > 0) {
    float current_v = current_mv / 1000.0f;
    this->publish_state(current_v);
  }
}

void AXP2101Number::dump_config() {
  LOG_NUMBER("", "AXP2101 Number", this);
  ESP_LOGCONFIG(TAG, "  Power Rail: %s", power_rail_to_string(this->rail_));
  ESP_LOGCONFIG(TAG, "  Voltage Range: %.2f-%.2fV (step: %.3fV)", this->min_mv_ / 1000.0f, this->max_mv_ / 1000.0f,
                this->step_mv_ / 1000.0f);
}

void AXP2101Number::control(float value) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set!");
    return;
  }

  // Convert volts to millivolts
  uint16_t millivolts = static_cast<uint16_t>(value * 1000.0f);

  // Validate range
  if (millivolts < this->min_mv_ || millivolts > this->max_mv_) {
    ESP_LOGW(TAG, "Voltage %.3fV out of range for %s (%.2f-%.2fV)", value, power_rail_to_string(this->rail_),
             this->min_mv_ / 1000.0f, this->max_mv_ / 1000.0f);
    return;
  }

  // Round to nearest step
  uint16_t offset = millivolts - this->min_mv_;
  uint16_t steps = (offset + this->step_mv_ / 2) / this->step_mv_;
  millivolts = this->min_mv_ + (steps * this->step_mv_);

  if (this->parent_->set_rail_voltage(this->rail_, millivolts)) {
    float actual_v = millivolts / 1000.0f;
    this->publish_state(actual_v);
    ESP_LOGD(TAG, "Set %s voltage to %.3fV", power_rail_to_string(this->rail_), actual_v);
  } else {
    ESP_LOGE(TAG, "Failed to set %s voltage to %.3fV", power_rail_to_string(this->rail_), value);
  }
}

}  // namespace axp2101
}  // namespace esphome
