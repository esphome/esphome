#pragma once

#include <array>

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::seeed_multi_channel_relay {

class SeeedMultiChannelRelay : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  /// Store a new I2C address in the board's EEPROM and talk to it there from now on.
  void change_address(uint8_t address);

  /// Switch relay `channel` (0-7) on or off; returns false when the write failed.
  bool set_relay(uint8_t channel, bool on);

#ifdef USE_SWITCH
  void register_switch(uint8_t channel, switch_::Switch *sw) { this->switches_[channel] = sw; }
#endif

 protected:
  uint8_t channels_{0};
  uint8_t firmware_version_{0};
#ifdef USE_SWITCH
  std::array<switch_::Switch *, 8> switches_{};
#endif
};

}  // namespace esphome::seeed_multi_channel_relay
