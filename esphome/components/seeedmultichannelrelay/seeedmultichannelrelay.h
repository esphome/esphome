#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace seeedmultichannelrelay {

static constexpr uint8_t CMD_CHANNEL_CTRL = 0x10;
static constexpr uint8_t CMD_SAVE_I2C_ADDR = 0x11;
static constexpr uint8_t CMD_READ_I2C_ADDR = 0x12;
static constexpr uint8_t CMD_READ_FIRMWARE_VER = 0x13;

enum class RelayBit : uint8_t {
  RELAY1 = 1,
  RELAY2 = 2,
  RELAY3 = 3,
  RELAY4 = 4,
  RELAY5 = 5,
  RELAY6 = 6,
  RELAY7 = 7,
  RELAY8 = 8
};

class SeeedMultiChannelRelay : public Component, public i2c::I2CDevice {
 public:
  void relay_write(uint8_t number, bool state);

  /*
    @brief Change device address from old_addr to new_addr.
    @param new_addr, the address to use.
          old_addr, the original address
    @return None
*/
  void change_i2c_address(uint8_t new_addr);

  /*
@brief Read firmware version from on board MCU
@param
@return Firmware version in byte
*/
  uint8_t get_firmware_version();

 protected:
  void write1_byte_(uint8_t register_address, uint8_t data);
  uint8_t read1_byte_(uint8_t register_address);
  uint8_t channel_state_;  // Value to save channel state
  uint8_t new_addr_;
  bool address_changed_ = false;

  /*
    @brief Control relay channels
    @param state, use one Byte to represent 8 channel
    @return None
  */
  void channel_ctrl_(uint8_t state);

  /*
    @brief Turn on one of 8 channels
    @param channel, channel to control with (range form 1 to 8)
    @return None
*/
  void turn_on_channel_(uint8_t channel);

  /*
      @brief Turn off on of 8 channels
      @param channel, channel to control with (range form 1 to 8)
      @return None
  */
  void turn_off_channel_(uint8_t channel);

  void dump_config() override;

  void setup() override;
};

}  // namespace seeedmultichannelrelay
}  // namespace esphome
