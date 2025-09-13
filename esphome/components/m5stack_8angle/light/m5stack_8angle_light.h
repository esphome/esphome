#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"

#include "../m5stack_8angle.h"

namespace esphome {
namespace m5stack_8angle {

static const uint8_t M5STACK_8ANGLE_NUM_LEDS = 9;
static const uint8_t M5STACK_8ANGLE_BYTES_PER_LED = 4;

class M5Stack8AngleLightOutput : public light::AddressableLight, public Parented<M5Stack8AngleComponent> {
 public:
  void setup() override;

  void write_state(light::LightState *state) override;

  int32_t size() const override { return M5STACK_8ANGLE_NUM_LEDS; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  };

  void clear_effect_data() override { memset(this->effect_data_, 0x00, M5STACK_8ANGLE_NUM_LEDS); };

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};
};

}  // namespace m5stack_8angle
}  // namespace esphome
