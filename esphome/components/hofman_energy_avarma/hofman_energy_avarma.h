#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace hofman_energy_avarma {

class HofmanEnergyAvarmaComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
};

}  // namespace hofman_energy_avarma
}  // namespace esphome
