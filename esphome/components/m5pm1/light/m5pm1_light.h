#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"

#include "../m5pm1.h"

namespace esphome::m5pm1 {

class M5PM1AddressableLight : public light::AddressableLight, public Parented<M5PM1Component> {
 public:
  void setup() override;

  void set_pin(M5PM1GPIOPin *pin) { this->pin_ = pin; }

  void write_state(light::LightState *state) override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_num_led(int32_t num) { this->num_leds_ = num; }

  int32_t size() const override { return this->num_leds_; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  };

  void clear_effect_data() override;

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;
  int32_t num_leds_{0};

  // The on device driver only support RGB565 (16 bits)
  // but the buffer is still RGB888 (24 bits) format for compatibility
  // data will be truncated when writing registers.
  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};

  // GPIO0 was passed from configuration
  M5PM1GPIOPin *pin_{nullptr};
};

}  // namespace esphome::m5pm1
