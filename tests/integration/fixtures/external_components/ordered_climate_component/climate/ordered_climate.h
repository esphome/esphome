#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"

namespace esphome::ordered_climate_test {

class OrderedClimate : public climate::Climate, public Component {
 public:
  void setup() override {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = 21.0f;
    this->publish_state();
  }

 protected:
  void control(const climate::ClimateCall &call) override {
    if (call.get_mode().has_value()) {
      this->mode = *call.get_mode();
    }
    if (call.get_target_temperature().has_value()) {
      this->target_temperature = *call.get_target_temperature();
    }
    if (call.get_fan_mode().has_value()) {
      this->set_fan_mode_(*call.get_fan_mode());
    }
    if (call.has_custom_fan_mode()) {
      this->set_custom_fan_mode_(call.get_custom_fan_mode());
    }
    this->publish_state();
  }

  climate::ClimateTraits traits() override {
    climate::ClimateTraits traits{};
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT});
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_HIGH);
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_LOW);
    return traits;
  }
};

}  // namespace esphome::ordered_climate_test
