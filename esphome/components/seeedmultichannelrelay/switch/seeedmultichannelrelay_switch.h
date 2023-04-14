#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

#include "esphome/components/seeedmultichannelrelay/seeedmultichannelrelay.h"

namespace esphome {
namespace seeedmultichannelrelay {

class SeeedMultiChannelRelaySwitch : public Component, public switch_::Switch, public Parented<SeeedMultiChannelRelay> {
 public:
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

}  // namespace seeedmultichannelrelay
}  // namespace esphome
