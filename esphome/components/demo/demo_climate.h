#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace demo {

enum class DemoClimateType {
  TYPE_1,
  TYPE_2,
  TYPE_3,
};

class DemoClimate : public climate::Climate, public Component {
 public:
  void set_type(DemoClimateType type) { type_ = type; }
  void setup() override {
    switch (type_) {
      case DemoClimateType::TYPE_1:
        this->current_temperature = 20.0;
        this->target_temperature = 21.0;
        this->mode = climate::CLIMATE_MODE_HEAT;
        this->action = climate::CLIMATE_ACTION_HEATING;
        break;
      case DemoClimateType::TYPE_2:
        this->target_temperature = 21.5;
        this->mode = climate::CLIMATE_MODE_AUTO;
        this->action = climate::CLIMATE_ACTION_COOLING;
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        this->custom_preset = {"My Preset"};
        break;
      case DemoClimateType::TYPE_3:
        this->current_temperature = 21.5;
        this->target_temperature_low = 21.0;
        this->target_temperature_high = 22.5;
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        this->custom_fan_mode = {"Auto Low"};
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
        this->preset = climate::CLIMATE_PRESET_AWAY;
        break;
    }
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
    if (call.get_target_temperature_low().has_value()) {
      this->target_temperature_low = *call.get_target_temperature_low();
    }
    if (call.get_target_temperature_high().has_value()) {
      this->target_temperature_high = *call.get_target_temperature_high();
    }
    if (call.get_fan_mode().has_value()) {
      this->fan_mode = *call.get_fan_mode();
      this->custom_fan_mode.reset();
    }
    if (call.get_swing_mode().has_value()) {
      this->swing_mode = *call.get_swing_mode();
    }
    if (call.get_custom_fan_mode().has_value()) {
      this->custom_fan_mode = *call.get_custom_fan_mode();
      this->fan_mode.reset();
    }
    if (call.get_preset().has_value()) {
      this->preset = *call.get_preset();
      this->custom_preset.reset();
    }
    if (call.get_custom_preset().has_value()) {
      this->custom_preset = *call.get_custom_preset();
      this->preset.reset();
    }
    this->publish_state();
  }
  climate::ClimateTraits traits() override {
    climate::ClimateTraits traits{};
    switch (type_) {
      case DemoClimateType::TYPE_1:
        traits.set_supports_current_temperature(true);
        traits.set_supported_modes({
            climate::CLIMATE_MODE_OFF,
            climate::CLIMATE_MODE_HEAT,
        });
        traits.set_supports_action(true);
        traits.set_visual_temperature_step(0.5);
        break;
      case DemoClimateType::TYPE_2:
        traits.set_supports_current_temperature(false);
        traits.set_supported_modes({
            climate::CLIMATE_MODE_OFF,
            climate::CLIMATE_MODE_HEAT,
            climate::CLIMATE_MODE_COOL,
            climate::CLIMATE_MODE_AUTO,
            climate::CLIMATE_MODE_DRY,
            climate::CLIMATE_MODE_FAN_ONLY,
        });
        traits.set_supports_action(true);
        traits.set_supported_fan_modes({
            climate::CLIMATE_FAN_ON,
            climate::CLIMATE_FAN_OFF,
            climate::CLIMATE_FAN_AUTO,
            climate::CLIMATE_FAN_LOW,
            climate::CLIMATE_FAN_MEDIUM,
            climate::CLIMATE_FAN_HIGH,
            climate::CLIMATE_FAN_MIDDLE,
            climate::CLIMATE_FAN_FOCUS,
            climate::CLIMATE_FAN_DIFFUSE,
        });
        traits.set_supported_custom_fan_modes({"Auto Low", "Auto High"});
        traits.set_supported_swing_modes({
            climate::CLIMATE_SWING_OFF,
            climate::CLIMATE_SWING_BOTH,
            climate::CLIMATE_SWING_VERTICAL,
            climate::CLIMATE_SWING_HORIZONTAL,
        });
        traits.set_supported_custom_presets({"My Preset"});
        break;
      case DemoClimateType::TYPE_3:
        traits.set_supports_current_temperature(true);
        traits.set_supports_two_point_target_temperature(true);
        traits.set_supported_modes({
            climate::CLIMATE_MODE_OFF,
            climate::CLIMATE_MODE_COOL,
            climate::CLIMATE_MODE_HEAT,
            climate::CLIMATE_MODE_HEAT_COOL,
        });
        traits.set_supported_custom_fan_modes({"Auto Low", "Auto High"});
        traits.set_supported_swing_modes({
            climate::CLIMATE_SWING_OFF,
            climate::CLIMATE_SWING_HORIZONTAL,
        });
        traits.set_supported_presets({
            climate::CLIMATE_PRESET_NONE,
            climate::CLIMATE_PRESET_HOME,
            climate::CLIMATE_PRESET_AWAY,
            climate::CLIMATE_PRESET_BOOST,
            climate::CLIMATE_PRESET_COMFORT,
            climate::CLIMATE_PRESET_ECO,
            climate::CLIMATE_PRESET_SLEEP,
            climate::CLIMATE_PRESET_ACTIVITY,
        });
        break;
    }
    return traits;
  }

  DemoClimateType type_;
};

}  // namespace demo
}  // namespace esphome
