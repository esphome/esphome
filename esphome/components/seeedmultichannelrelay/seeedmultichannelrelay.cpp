#include "seedmultichannelrelay.h"
#include "esphome/core/log.h"
#include "seeedmultichannelrelay.h"

namespace esphome {
namespace seedmultichannelrelay {

static const char *const TAG = "SeedMultiChannelRelay";

void SeeedMultiChannelRelay::dump_config() {
  ESP_LOGCONFIG(TAG, "Seed Multi Channel Relays:");
  LOG_I2C_DEVICE(this);
}

/*! @brief Setting the mode of the device, and turn off all relays.
 *  @param mode Async = 0, Sync = 1. */
void SeeedMultiChannelRelay::init_(bool mode) {
  this->write1_byte_(UNIT_4RELAY_REG, mode);
  this->write1_byte_(UNIT_4RELAY_RELAY_REG, 0);
}

/*! @brief Read a certain length of data to the specified register address. */
uint8_t SeeedMultiChannelRelay::read1_byte_(uint8_t register_address) {
  uint8_t data;
  if (!this->read_byte(register_address, &data)) {
    ESP_LOGW(TAG, "Read from relay failed!");
    this->status_set_warning();
    return uint8_t(0);
  }
  return data;
}

/*! @brief Control the on/off of the specified relay.
 *  @param number Bit number of relay (0~3).
    @param state OFF = 0, ON = 1 . */
void SeeedMultiChannelRelay::relay_write(uint8_t number, bool state) {
  uint8_t state_from_device = this->read1_byte_(UNIT_4RELAY_RELAY_REG);
  if (state == 0) {
    state_from_device &= ~(0x01 << number);
  } else {
    state_from_device |= (0x01 << number);
  }
  this->write1_byte_(UNIT_4RELAY_RELAY_REG, state_from_device);
}

void SeeedMultiChannelRelay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Seeed Multi Channel Relay...");
  uint8_t setupmode = 1;
  this->init_(setupmode);
}

void SeeedMultiChannelRelay::change_i2c_address(uint8_t new_addr, uint8_t old_addr) {}

uint8_t SeeedMultiChannelRelay::get_firmware_version(void) { return uint8_t(); }

/*! @brief Write a certain length of data to the specified register address. */
void SeeedMultiChannelRelay::write1_byte_(uint8_t register_address, uint8_t data) {
  if (!this->write_byte(register_address, data)) {
    ESP_LOGW(TAG, "Write to relay failed!");
    this->status_set_warning();
    return;
  }
}

}  // namespace seedmultichannelrelay
}  // namespace esphome
