#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

#include "esphome/components/seeedmultichannelrelay/seeed_multi_channel_relay.h"

namespace esphome {
namespace seeedmultichannelrelay {

class Seeed_Multi_Channel_Relay_Switch : public Component, public switch_::Switch, public Parented<seeed_multi_channel_relay> {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(bool state) override;

  void set_channel(uint8_t channel) { this->channel_ = channel; }
#ifdef USE_SWITCH_INTERLOCK
  void set_interlock(const std::vector<Switch *> &interlock);
  void set_interlock_wait_time(uint32_t interlock_wait_time) { interlock_wait_time_ = interlock_wait_time; }
#endif
 protected:
  uint8_t channel_;
#ifdef USE_SWITCH_INTERLOCK
  std::vector<Switch *> interlock_;
  uint32_t interlock_wait_time_{0};
#endif
};

}  // namespace seeed_multi_channel_relay
}  // namespace esphome
