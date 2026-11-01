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
      : internal_accessor_(this->color_data_, this->effect_data_, {0, 1, 2, light::ChannelColors::RESERVED_WHITE},
                           &this->correction_) {
    memset(this->color_data_, 0xFF, M5STACK_8ANGLE_NUM_LEDS * M5STACK_8ANGLE_BYTES_PER_LED);
    this->accessor_ = &this->internal_accessor_;
  }

  void write_state(light::LightState *state) override;

  int32_t size() const override { return M5STACK_8ANGLE_NUM_LEDS; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  };

  void clear_effect_data() override { this->internal_accessor_.clear_effect_data(M5STACK_8ANGLE_NUM_LEDS); }

 protected:
  uint8_t color_data_[M5STACK_8ANGLE_NUM_LEDS * M5STACK_8ANGLE_BYTES_PER_LED];
  uint8_t effect_data_[M5STACK_8ANGLE_NUM_LEDS] = {};
  light::CorrectedBufferColorAccessor internal_accessor_;
};

}  // namespace esphome::m5stack_8angle
