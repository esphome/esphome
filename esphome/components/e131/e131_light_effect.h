#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/light/light_effect.h"
#include "esphome/components/light/light_output.h"
#include "e131_light_effect_base.h"

namespace esphome {
namespace e131 {

class E131LightEffect : public E131LightEffectBase, public light::LightEffect {
 public:
  E131LightEffect(const std::string &name);

  const std::string &get_name() override;

  void start() override;
  void stop() override;
  void apply() override;

  int get_universe_count() const override;

 protected:
  bool process(int universe, const E131Packet &packet) override;
};

}  // namespace e131
}  // namespace esphome

#endif  // ESPHOME_E131_LIGHT_EFFECT_H
