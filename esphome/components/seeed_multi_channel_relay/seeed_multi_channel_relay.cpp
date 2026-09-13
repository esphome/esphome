#include "seeed_multi_channel_relay.h"
#include "esphome/core/log.h"

namespace esphome::seeed_multi_channel_relay {

static const char *const TAG = "seeed_multi_channel_relay";

void seeed_multi_channel_relay::channel_ctrl(uint8_t state) {
  this->channel_state_ = state;
  this->write1_byte(CMD_CHANNEL_CTRL, state);
}

void seeed_multi_channel_relay::turn_on_channel(uint8_t channel) {
  this->channel_state |= (1 << (channel - 1));
  this->channel_ctrl(channel_state);
}

void seeed_multi_channel_relay::turn_off_channel(uint8_t channel) {
  this->channel_state &= ~(1 << (channel - 1));
  this->channel_ctrl(channel_state);
}

void seeed_multi_channel_relay::dump_config() {
  ESP_LOGCONFIG(TAG, "Seeed Multi Channel Relays:");
  LOG_I2C_DEVICE(this);
}

/*! @brief Read a certain length of data to the specified register address. */
uint8_t seeed_multi_channel_relay::read1_byte(uint8_t register_address) {
  uint8_t data;
  if (!this->read_byte(register_address, &data)) {
    ESP_LOGW(TAG, "Read from relay failed!");
    this->status_set_warning();
    return uint8_t(0);
  }
  return data;
}

/*! @brief Control the on/off of the specified relay.
  *  @param number Bit number of relay (1~8).
    @param state OFF = 0, ON = 1 . */
void seeed_multi_channel_relay::relay_write(uint8_t number, bool state) {
  if (state) {
    this->turn_on_channel(number);
  } else {
    this->turn_off_channel(number);
  }
}

void seeed_multi_channel_relay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Seeed Multi Channel Relay...");
  ESP_LOGCONFIG(TAG, "Firmware version of the Seeed Multi Channel Relay %u", this->get_firmware_version());
  if (this->address_changed) {
    this->write1_byte(CMD_SAVE_I2C_ADDR, this->new_addr);
    this->set_i2c_address(this->new_addr);
    ESP_LOGCONFIG(TAG, "I2C address of control changed to %u", this->new_addr);
  }
}

void seeed_multi_channel_relay::change_i2c_address(uint8_t new_addr) {
  this->new_addr = new_addr;
  address_changed = true;
}

uint8_t seeed_multi_channel_relay::get_firmware_version() {
  uint8_t firmware_from_device = this->read1_byte(CMD_READ_FIRMWARE_VER);
  return firmware_from_device;
}

/*! @brief Write a certain length of data to the specified register address. */
void seeed_multi_channel_relay::write1_byte_(uint8_t register_address, uint8_t data) {
  if (!this->write_byte(register_address, data)) {
    ESP_LOGW(TAG, "Write to relay failed!");
    this->status_set_warning();
    return;
  }
}

}  // namespace esphome::seeed_multi_channel_relay
