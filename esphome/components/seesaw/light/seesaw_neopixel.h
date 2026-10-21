#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawNeopixel : public light::AddressableLight, public Parented<Seesaw> {
 public:
  explicit SeesawNeopixel(int pin) : pin_(pin) {}

  void setup() override;
  void set_num_leds(uint16_t num_leds);
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;
  void clear_effect_data() override;
  int32_t size() const override { return this->num_leds_; }

 protected:
  const int pin_;
  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};
  uint16_t num_leds_{0};

  light::ESPColorView get_view_internal(int32_t index) const override {
    uint8_t *base = this->buf_ + (3 * index);
    return light::ESPColorView(base + 0, base + 1, base + 2, nullptr, this->effect_data_ + index, &this->correction_);
  }
};

}  // namespace esphome::seesaw
