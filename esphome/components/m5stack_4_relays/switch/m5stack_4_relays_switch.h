#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

#include "esphome/components/M5Stack_4_Relays/M5Stack_4_Relays.h"

namespace esphome {
namespace M5Stack_4_Relays {

class M5Stack_Switch : public Component, public switch_::Switch, public Parented<M5Stack_4_Relays> {
 public:
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;
  void write_state(bool state) override;

  void set_channel(RelayBit channel) { this->channel_ = (uint8_t) channel; }

  void set_interlock(const std::vector<Switch *> &interlock);
  void set_interlock_wait_time(uint32_t interlock_wait_time) { interlock_wait_time_ = interlock_wait_time; }

 protected:
  uint8_t channel_;
  std::vector<Switch *> interlock_;
  uint32_t interlock_wait_time_{0};
};

}  // namespace M5Stack_4_Relays
}  // namespace esphome
