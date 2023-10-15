#pragma once

#include "esphome/core/component.h"
#include "../econet.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace econet {

class EconetClimate : public climate::Climate, public Component, public EconetClient {
 public:
  void setup() override;
  void dump_config() override;
  void set_current_temperature_id(const std::string &current_temperature_id) {
    current_temperature_id_ = current_temperature_id;
  }
  void set_target_temperature_id(const std::string &target_temperature_id) {
    target_temperature_id_ = target_temperature_id;
  }
  void set_target_temperature_low_id(const std::string &target_temperature_low_id) {
    target_temperature_low_id_ = target_temperature_low_id;
  }
  void set_target_temperature_high_id(const std::string &target_temperature_high_id) {
    target_temperature_high_id_ = target_temperature_high_id;
  }
  void set_mode_id(const std::string &mode_id) { mode_id_ = mode_id; }
  void set_custom_preset_id(const std::string &custom_preset_id) { custom_preset_id_ = custom_preset_id; }
  void set_custom_fan_mode_id(const std::string &custom_fan_mode_id) { custom_fan_mode_id_ = custom_fan_mode_id; }
  void set_modes(const std::vector<uint8_t> &keys, const std::vector<climate::ClimateMode> &values) {
    std::transform(keys.begin(), keys.end(), values.begin(), std::inserter(modes_, modes_.end()),
                   [](uint8_t k, climate::ClimateMode v) { return std::make_pair(k, v); });
  }
  void set_custom_presets(const std::vector<uint8_t> &keys, const std::vector<std::string> &values) {
    std::transform(keys.begin(), keys.end(), values.begin(), std::inserter(custom_presets_, custom_presets_.end()),
                   [](uint8_t k, std::string v) { return std::make_pair(k, v); });
  }
  void set_custom_fan_modes(const std::vector<uint8_t> &keys, const std::vector<std::string> &values) {
    std::transform(keys.begin(), keys.end(), values.begin(), std::inserter(custom_fan_modes_, custom_fan_modes_.end()),
                   [](uint8_t k, std::string v) { return std::make_pair(k, v); });
  }

 protected:
  std::string current_temperature_id_{""};
  std::string target_temperature_id_{""};
  std::string target_temperature_low_id_{""};
  std::string target_temperature_high_id_{""};
  std::string mode_id_{""};
  std::string custom_preset_id_{""};
  std::string custom_fan_mode_id_{""};
  std::map<uint8_t, climate::ClimateMode> modes_;
  std::map<uint8_t, std::string> custom_presets_;
  std::map<uint8_t, std::string> custom_fan_modes_;
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
};

}  // namespace econet
}  // namespace esphome
