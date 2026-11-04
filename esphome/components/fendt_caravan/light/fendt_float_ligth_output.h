
#pragma once
#ifdef USE_ESP32
#include "fendt_light_output.h"

namespace esphome::fendt_caravan {

class FendtFloatLightOutput : public FendtLightOutput {
 public:
  FendtFloatLightOutput() : FendtLightOutput(light::ColorMode::BRIGHTNESS) {};
  void write_state(light::LightState *state) override;
};
}  // namespace esphome::fendt_caravan
#endif
