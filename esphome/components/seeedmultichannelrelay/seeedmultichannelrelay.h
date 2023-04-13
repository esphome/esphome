#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace seedmultichannelrelay {

static constexpr uint8_t UNIT_4RELAY_REG = 0X10;
static constexpr uint8_t UNIT_4RELAY_RELAY_REG = 0X11;

static constexpr uint8_t CHANNLE1_BIT = 0x01;
static constexpr uint8_t CHANNLE2_BIT = 0x02;
static constexpr uint8_t CHANNLE3_BIT = 0x04;
static constexpr uint8_t CHANNLE4_BIT = 0x08;
static constexpr uint8_t CHANNLE5_BIT = 0x10;
static constexpr uint8_t CHANNLE6_BIT = 0x20;
static constexpr uint8_t CHANNLE7_BIT = 0x40;
static constexpr uint8_t CHANNLE8_BIT = 0x80;

static constexpr uint8_t CMD_CHANNEL_CTRL = 0x10;
static constexpr uint8_t CMD_SAVE_I2C_ADDR = 0x11;
static constexpr uint8_t CMD_READ_I2C_ADDR = 0x12;
static constexpr uint8_t CMD_READ_FIRMWARE_VER = 0x13;

enum class RelayBit : uint8_t {
  CHANNLE1_BIT = 0x01,
  CHANNLE2_BIT = 0x02,
  CHANNLE3_BIT = 0x04,
  CHANNLE4_BIT = 0x08,
  CHANNLE5_BIT = 0x10,
  CHANNLE6_BIT = 0x20,
  CHANNLE7_BIT = 0x40,
  CHANNLE8_BIT = 0x80
};

class SeeedMultiChannelRelay : public Component, public i2c::I2CDevice {
 public:
  void relay_write(uint8_t number, bool state);

  /**
    @brief Change device address from old_addr to new_addr.
    @param new_addr, the address to use.
          old_addr, the original address
    @return None
*/
  void change_i2c_address(uint8_t new_addr, uint8_t old_addr);

  /**
@brief Read firmware version from on board MCU
@param
@return Firmware version in byte
*/
  uint8_t get_firmware_version(void);

 protected:
  void write1_byte_(uint8_t register_address, uint8_t data);
  uint8_t read1_byte_(uint8_t register_address);

  void dump_config() override;

  void init_(bool mode);

  void setup() override;
};

}  // namespace seedmultichannelrelay
}  // namespace esphome
