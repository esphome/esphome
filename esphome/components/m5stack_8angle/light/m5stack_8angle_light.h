#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"

#include "../m5stack_8angle.h"

namespace esphome::m5stack_8angle {

static const uint8_t M5STACK_8ANGLE_NUM_LEDS = 9;
static const uint8_t M5STACK_8ANGLE_BYTES_PER_LED = 4;

class M5Stack8AngleLightOutput final : public light::AddressableLight, public Parented<M5Stack8AngleComponent> {
 public:
  M5Stack8AngleLightOutput()
      : buffer_(M5STACK_8ANGLE_NUM_LEDS,
                {.channel_colors = {.r = 0, .g = 1, .b = 2}, .bytes_per_led = M5STACK_8ANGLE_BYTES_PER_LED},
                &this->correction_) {}

  light::ESPColorBuffer &buffer() override { return this->buffer_; }

  void setup() override;

  void write_state(light::LightState *state) override;

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  };

 protected:
  light::InterleavedColorBuffer buffer_;
};

}  // namespace esphome::m5stack_8angle
