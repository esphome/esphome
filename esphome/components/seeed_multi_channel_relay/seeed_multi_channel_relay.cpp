#include "seeed_multi_channel_relay.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::seeed_multi_channel_relay {

ESPHOME_LOG_TAG(TAG, "seeed_multi_channel_relay");

static constexpr uint8_t CMD_CHANNEL_CTRL = 0x10;
static constexpr uint8_t CMD_SAVE_I2C_ADDR = 0x11;
static constexpr uint8_t CMD_READ_FIRMWARE_VER = 0x13;

void SeeedMultiChannelRelay::setup() {
  if (!this->read_byte(CMD_READ_FIRMWARE_VER, &this->firmware_version_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
#ifdef USE_SWITCH
  // channels_ owns all 8 relays and the board has no read-back: after an ESP-only reboot the board keeps its
  // relays, so every channel is set from the switches' restore modes in one write, and a channel already on
  // never drops out while the others are written; a power cycle starts with all off. A switch with restore_mode
  // DISABLED is driven off too, so its state matches what the next write does
  uint8_t value = 0;
  for (uint8_t channel = 0; channel != this->switches_.size(); channel++) {
    switch_::Switch *sw = this->switches_[channel];
    if (sw == nullptr)
      continue;
    const bool on = sw->get_initial_write_state().value_or(false);
    assign_bits(value, 1 << channel, on);
  }
  if (!this->status_comm_result(this->write_byte(CMD_CHANNEL_CTRL, value)))
    return;
  this->channels_ = value;
  for (uint8_t channel = 0; channel != this->switches_.size(); channel++) {
    PUBLISH_STATE(this->switches_[channel], (value >> channel) & 1);
  }
#endif
}

void SeeedMultiChannelRelay::change_address(uint8_t address) {
  if (this->is_failed())
    return;
  // A lambda bypasses the schema range; a reserved address would strand the board
  if (address < 0x08 || address > 0x77) {
    ESP_LOGW(TAG, "I2C address 0x%02X is reserved; not stored", address);
    return;
  }
  if (!this->status_comm_result(this->write_byte(CMD_SAVE_I2C_ADDR, address)))
    return;
  this->set_i2c_address(address);
  ESP_LOGI(TAG, "Board now answers at 0x%02X; update address: in the configuration", address);
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
  PUBLISH_STATE(this->switches_[channel], on);
#endif
  return true;
}

}  // namespace esphome::seeed_multi_channel_relay
