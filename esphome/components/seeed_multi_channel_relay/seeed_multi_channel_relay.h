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

  /// Store a new I2C address in the board's EEPROM during setup.
  void set_change_address_to(uint8_t address) { this->change_address_to_ = address; }

  /// Switch relay `channel` (0-7) on or off; returns false when the write failed.
  bool set_relay(uint8_t channel, bool on);

#ifdef USE_SWITCH
  void register_switch(uint8_t channel, switch_::Switch *sw) { this->switches_[channel] = sw; }
#endif

 protected:
  bool read_firmware_version_();
  void apply_address_change_();

  uint8_t channels_{0};
  uint8_t change_address_to_{0};
  uint8_t firmware_version_{0};
#ifdef USE_SWITCH
  std::array<switch_::Switch *, 8> switches_{};
#endif
};

}  // namespace esphome::seeed_multi_channel_relay
