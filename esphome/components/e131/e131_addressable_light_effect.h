#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light_effect.h"
#include "e131_light_effect_base.h"

namespace esphome {
namespace e131 {

class E131AddressableLightEffect : public E131LightEffectBase, public light::AddressableLightEffect {
 public:
  E131AddressableLightEffect(const std::string &name);

  const std::string &get_name() override;

  void start() override;
  void stop() override;

  int get_universe_count() const override;

  void apply(light::AddressableLight &it, const Color &current_color) override;

 protected:
  bool process(int universe, const E131Packet &packet) override;
};

}  // namespace e131
}  // namespace esphome

#endif  // USE_ARDUINO
