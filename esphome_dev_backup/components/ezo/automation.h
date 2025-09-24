#pragma once
#include <utility>

#include "esphome/core/automation.h"
#include "ezo.h"

namespace esphome {
namespace ezo {

class LedTrigger : public Trigger<bool> {
 public:
  explicit LedTrigger(EZOSensor *ezo) {
    ezo->add_led_state_callback([this](bool value) { this->trigger(value); });
  }
};

class CustomTrigger : public Trigger<std::string> {
 public:
  explicit CustomTrigger(EZOSensor *ezo) {
    ezo->add_custom_callback([this](std::string value) { this->trigger(std::move(value)); });
  }
};

class TTrigger : public Trigger<std::string> {
 public:
  explicit TTrigger(EZOSensor *ezo) {
    ezo->add_t_callback([this](std::string value) { this->trigger(std::move(value)); });
  }
};

class CalibrationTrigger : public Trigger<std::string> {
 public:
  explicit CalibrationTrigger(EZOSensor *ezo) {
    ezo->add_calibration_callback([this](std::string value) { this->trigger(std::move(value)); });
  }
};

class SlopeTrigger : public Trigger<std::string> {
 public:
  explicit SlopeTrigger(EZOSensor *ezo) {
    ezo->add_slope_callback([this](std::string value) { this->trigger(std::move(value)); });
  }
};

class DeviceInformationTrigger : public Trigger<std::string> {
 public:
  explicit DeviceInformationTrigger(EZOSensor *ezo) {
    ezo->add_device_infomation_callback([this](std::string value) { this->trigger(std::move(value)); });
  }
};

}  // namespace ezo
}  // namespace esphome
