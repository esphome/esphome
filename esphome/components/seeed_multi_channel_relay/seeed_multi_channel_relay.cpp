#include "seeed_multi_channel_relay.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::seeed_multi_channel_relay {

ESPHOME_LOG_TAG(TAG, "seeed_multi_channel_relay");

static constexpr uint8_t CMD_CHANNEL_CTRL = 0x10;
static constexpr uint8_t CMD_SAVE_I2C_ADDR = 0x11;
static constexpr uint8_t CMD_READ_FIRMWARE_VER = 0x13;

void SeeedMultiChannelRelay::setup() {
  if (this->change_address_to_ != 0 && this->change_address_to_ != this->address_)
    this->apply_address_change_();
  if (!this->read_firmware_version_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
#ifdef USE_SWITCH
  // The board has no state read-back and powers up with every relay off, so restore_mode decides
  for (switch_::Switch *sw : this->switches_) {
    if (sw != nullptr)
      sw->restore();
  }
#endif
}

void SeeedMultiChannelRelay::apply_address_change_() {
  const uint8_t old_address = this->address_;
  // A previous boot may already have stored the new address
  this->set_i2c_address(this->change_address_to_);
  if (this->read_firmware_version_())
    return;
  this->set_i2c_address(old_address);
  if (this->write_byte(CMD_SAVE_I2C_ADDR, this->change_address_to_)) {
    this->set_i2c_address(this->change_address_to_);
    ESP_LOGI(TAG, "I2C address changed from 0x%02X to 0x%02X", old_address, this->change_address_to_);
  }
}

bool SeeedMultiChannelRelay::read_firmware_version_() {
  return this->read_byte(CMD_READ_FIRMWARE_VER, &this->firmware_version_);
}

void SeeedMultiChannelRelay::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Seeed Multi Channel Relay:\n"
                "  Firmware version: %u",
                this->firmware_version_);
  LOG_I2C_DEVICE(this);
}

bool SeeedMultiChannelRelay::set_relay(uint8_t channel, bool on) {
  if (this->is_failed())
    return false;
  uint8_t value = this->channels_;
  assign_bits(value, 1 << channel, on);
  if (!this->status_comm_result(this->write_byte(CMD_CHANNEL_CTRL, value)))
    return false;
  this->channels_ = value;
#ifdef USE_SWITCH
  if (this->switches_[channel] != nullptr)
    this->switches_[channel]->publish_state(on);
#endif
  return true;
}

}  // namespace esphome::seeed_multi_channel_relay
