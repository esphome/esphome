#pragma once
#ifdef USE_ESP32
#include "esphome/components/fendt_caravan/lighting_device_hub.h"
#include "esphome/components/fendt_caravan/device_commands.h"
#include "esphome/components/fendt_caravan/device_decoders.h"
#include "esphome/components/fendt_caravan/variable.h"
#include "esphome/components/light/light_output.h"

namespace esphome::fendt_caravan {

class FendtLightOutput : public light::LightOutput, public Component, public Parented<LightingDeviceHub> {
 public:
  FendtLightOutput() { this->color_mode_ = light::ColorMode::ON_OFF; }
  explicit FendtLightOutput(light::ColorMode color_mode) { this->color_mode_ = color_mode; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({this->color_mode_});
    return traits;
  }

  void set_variable_name(const std::string &var_name) { this->variable_name_ = var_name; }
  void setup_state(light::LightState *state) override = 0;
  void write_state(light::LightState *state) override = 0;

 protected:
  light::LightState *light_state_{nullptr};
  Variable<int> *variable_{nullptr};
  std::string variable_name_{};

 private:
  light::ColorMode color_mode_ = light::ColorMode::ON_OFF;
};

}  // namespace esphome::fendt_caravan
#endif
