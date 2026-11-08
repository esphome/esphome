
#pragma once
#ifdef USE_ESP32
#include "fendt_light_output.h"

namespace esphome::fendt_caravan {

class FendtFloatLightOutput : public FendtLightOutput {
 public:
  FendtFloatLightOutput() : FendtLightOutput(light::ColorMode::BRIGHTNESS) {};
  void write_state(light::LightState *state) override {
    this->parent_->write_state(this->variable_name_.c_str(), state);
  };
};
}  // namespace esphome::fendt_caravan
#endif
