#include "seeedmultichannelrelay.h"
#include "esphome/core/log.h"
#include "seeedmultichannelrelay.h"

namespace esphome {
namespace seeedmultichannelrelay {

static const char *const TAG = "SeeedMultiChannelRelay";

void SeeedMultiChannelRelay::channel_ctrl_(uint8_t state) {
  this->channel_state_ = state;
  this->write1_byte_(CMD_CHANNEL_CTRL, state);
}

void SeeedMultiChannelRelay::turn_on_channel_(uint8_t channel) {
  this->channel_state_ |= (1 << (channel - 1));
  this->channel_ctrl_(channel_state_);
}

void SeeedMultiChannelRelay::turn_off_channel_(uint8_t channel) {
  this->channel_state_ &= ~(1 << (channel - 1));
  this->channel_ctrl_(channel_state_);
}

void SeeedMultiChannelRelay::dump_config() {
  ESP_LOGCONFIG(TAG, "Seeed Multi Channel Relays:");
  LOG_I2C_DEVICE(this);
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
  if (state) {
    this->turn_on_channel_(number);
  } else {
    this->turn_off_channel_(number);
  }
}

void SeeedMultiChannelRelay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Seeed Multi Channel Relay...");
  ESP_LOGCONFIG(TAG, "Firmware version of the Seeed Multi Channel Relay %u", this->get_firmware_version());
  if (this->address_changed_) {
    this->write1_byte_(CMD_SAVE_I2C_ADDR, this->new_addr_);
    this->set_i2c_address(this->new_addr_);
    ESP_LOGCONFIG(TAG, "I2C address of control changed to %u", this->new_addr_);
  }
}

void SeeedMultiChannelRelay::change_i2c_address(uint8_t new_addr) {
  this->new_addr_ = new_addr;
  address_changed_ = true;
}

uint8_t SeeedMultiChannelRelay::get_firmware_version() {
  uint8_t firmware_from_device = this->read1_byte_(CMD_READ_FIRMWARE_VER);
  return firmware_from_device;
}

/*! @brief Write a certain length of data to the specified register address. */
void SeeedMultiChannelRelay::write1_byte_(uint8_t register_address, uint8_t data) {
  if (!this->write_byte(register_address, data)) {
    ESP_LOGW(TAG, "Write to relay failed!");
    this->status_set_warning();
    return;
  }
}

}  // namespace seeedmultichannelrelay
}  // namespace esphome
