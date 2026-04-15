#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"

namespace esphome::legacy_climate_test {

/// Test climate that uses the DEPRECATED ClimateTraits setters for custom modes.
/// This validates backward compatibility for external components that haven't migrated.
class LegacyClimate : public climate::Climate, public Component {
 public:
  void setup() override {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = 22.0f;
    this->current_temperature = 20.0f;
    this->publish_state();
  }

 protected:
  climate::ClimateTraits traits() override {
    auto traits = climate::ClimateTraits();
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_COOL});
    traits.set_visual_min_temperature(16.0f);
    traits.set_visual_max_temperature(30.0f);
    traits.set_visual_temperature_step(0.5f);

    // DEPRECATED API: setting custom modes directly on ClimateTraits.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    traits.set_supported_custom_fan_modes({"Turbo", "Silent", "Auto"});
    traits.set_supported_custom_presets({"Eco Mode", "Night Mode"});
#pragma GCC diagnostic pop

    return traits;
  }

  void control(const climate::ClimateCall &call) override {
    if (call.get_mode().has_value()) {
      this->mode = *call.get_mode();
    }
    if (call.get_target_temperature().has_value()) {
      this->target_temperature = *call.get_target_temperature();
    }
    if (call.has_custom_fan_mode()) {
      this->set_custom_fan_mode_(call.get_custom_fan_mode());
    }
    if (call.has_custom_preset()) {
      this->set_custom_preset_(call.get_custom_preset());
    }
    this->publish_state();
  }
};

}  // namespace esphome::legacy_climate_test
