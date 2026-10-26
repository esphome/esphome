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
      : num_leds_(num_leds),
        buf_(new uint8_t[num_leds * 4]()),
        effect_data_(new uint8_t[num_leds]()),
        internal_accessor_(this->buf_.get(), this->effect_data_.get(),
                           light::ChannelColors{.r = 0, .g = 1, .b = 2, .w = 3}, &this->correction_) {
    this->accessor_ = &this->internal_accessor_;
  }

  void setup() override {}
  void write_state(light::LightState *state) override {}
  int32_t size() const override { return this->num_leds_; }
  void clear_effect_data() override {
    for (uint16_t i = 0; i < this->num_leds_; i++)
      this->effect_data_[i] = 0;
  }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  // Accessors for tests: return the raw stored byte (post gamma correction),
  // which is what actual LED hardware would receive.
  uint8_t get_raw_red(uint16_t index) const { return this->buf_[index * 4 + 0]; }
  uint8_t get_raw_green(uint16_t index) const { return this->buf_[index * 4 + 1]; }
  uint8_t get_raw_blue(uint16_t index) const { return this->buf_[index * 4 + 2]; }
  uint8_t get_raw_white(uint16_t index) const { return this->buf_[index * 4 + 3]; }

 protected:
  uint16_t num_leds_;
  std::unique_ptr<uint8_t[]> buf_;
  std::unique_ptr<uint8_t[]> effect_data_;
  light::CorrectedBufferColorAccessor internal_accessor_;
};

}  // namespace esphome::mock_addressable_light
