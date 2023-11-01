#pragma once

#include "esphome/core/component.h"

#include "e131_addressable_light_effect.h"

namespace esphome {
namespace e131 {

class E131Component;
struct E131Packet;

class E131BasicLightEffect : public E131AddressableLightEffect {
 public:
  E131BasicLightEffect(const std::string &name);

  int get_lights_per_universe() const override;

 protected:
  bool process_(int universe, const E131Packet &packet) override;
};

}  // namespace e131
}  // namespace esphome
