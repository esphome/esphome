#pragma once

#include "../neokey.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome {
namespace neokey {

#define NUM_BYTES_PER_LED 3

class NeoKeyLight : public light::AddressableLight {
 public:
  void setup() override;
  void set_neokey(NeoKeyComponent *neokey);
  int32_t size() const override;
  void write_state(light::LightState *state) override;
  void clear_effect_data() override;
  light::LightTraits get_traits() override;
 protected:
  NeoKeyComponent *neokey_;

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};

  light::ESPColorView get_view_internal(int32_t index) const override;
};

}  // namespace neokey
}  // namespace esphome