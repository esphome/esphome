#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawNeopixel : public light::AddressableLight, public Parented<Seesaw> {
 public:
  /// buf holds 4 bytes per LED: GRB in the order the strip takes it, then the effect data
  SeesawNeopixel(uint8_t pin, uint16_t num_leds, uint8_t *buf) : buf_(buf), num_leds_(num_leds), pin_(pin) {}

  void setup() override;
  void dump_config() override;
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;
  void clear_effect_data() override;
  int32_t size() const override { return this->num_leds_; }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    uint8_t *base = this->buf_ + 3 * index;
    return light::ESPColorView(base + 1, base + 0, base + 2, nullptr, this->buf_ + 3 * this->num_leds_ + index,
                               &this->correction_);
  }

  uint8_t *const buf_;
  const uint16_t num_leds_;
  const uint8_t pin_;
};

}  // namespace esphome::seesaw
