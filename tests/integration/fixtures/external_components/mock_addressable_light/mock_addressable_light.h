#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "esphome/components/light/addressable_light.h"
#include "esphome/core/component.h"

namespace esphome::mock_addressable_light {

// In-memory addressable light for host-mode integration tests. Exposes the raw
// per-LED byte buffer (post-gamma-correction, as the hardware would see it)
// so tests can observe transition behavior without real hardware.
class MockAddressableLight : public light::AddressableLight {
 public:
  explicit MockAddressableLight(uint16_t num_leds)
      : buffer_(num_leds, {.channel_colors = {.r = 0, .g = 1, .b = 2, .w = 3}}, &this->correction_) {
    RAMAllocator<uint8_t> allocator;
    buffer_.allocate_and_setup(&allocator);
  }

  light::ESPColorBuffer &buffer() override { return buffer_; }
  void setup() override {}
  void write_state(light::LightState *state) override {}
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  // Accessors for tests: return the raw stored byte (post gamma correction),
  // which is what actual LED hardware would receive.
  uint8_t get_raw_red(uint16_t index) const { return this->buffer_.get_led_data()[index * 4 + 0]; }
  uint8_t get_raw_green(uint16_t index) const { return this->buffer_.get_led_data()[index * 4 + 1]; }
  uint8_t get_raw_blue(uint16_t index) const { return this->buffer_.get_led_data()[index * 4 + 2]; }
  uint8_t get_raw_white(uint16_t index) const { return this->buffer_.get_led_data()[index * 4 + 3]; }

 protected:
  light::PackedColorBuffer buffer_;
};

}  // namespace esphome::mock_addressable_light
