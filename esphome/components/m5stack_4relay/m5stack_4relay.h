#pragma once

#include <array>

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::m5stack_4relay {

class M5Stack4Relay : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  /// Switch relay `channel` (0-3) on or off; returns false when the write failed.
  bool set_relay(uint8_t channel, bool on);

#ifdef USE_SWITCH
  void register_switch(uint8_t channel, switch_::Switch *sw) { this->switches_[channel] = sw; }
#endif

 protected:
  uint8_t relays_{0};
#ifdef USE_SWITCH
  std::array<switch_::Switch *, 4> switches_{};
#endif
};

}  // namespace esphome::m5stack_4relay
