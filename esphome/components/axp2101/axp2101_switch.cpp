/**
 * @file axp2101_switch.cpp
 * @brief Implementation of AXP2101 switch component
 */
#include "axp2101_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace axp2101 {

static const char *const TAG = "axp2101.switch";

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

void AXP2101Switch::dump_config() {
  LOG_SWITCH("", "AXP2101 Switch", this);
  ESP_LOGCONFIG(TAG, "  Power Rail: %s", power_rail_to_string(this->rail_));
}

void AXP2101Switch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set!");
    return;
  }

  bool success;
  if (state) {
    success = this->parent_->enable_power_rail(this->rail_);
  } else {
    success = this->parent_->disable_power_rail(this->rail_);
  }

  if (success) {
    this->publish_state(state);
  } else {
    ESP_LOGE(TAG, "Failed to %s power rail %s", state ? "enable" : "disable", power_rail_to_string(this->rail_));
  }
}

}  // namespace axp2101
}  // namespace esphome
