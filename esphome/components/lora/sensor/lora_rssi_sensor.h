#pragma once
#include "../lora_component.h"

namespace esphome {
namespace lora {

class LoraRSSISensor : public sensor::Sensor, public PollingComponent {
 public:
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void register_lora(LoraComponent *lora) { this->lora_ = lora; }

 protected:
  LoraComponent *lora_;
};

}  // namespace lora
}  // namespace esphome
