#include "ds2482.h"

namespace esphome {
namespace ds2482 {
static const char *const TAG = "ds2482.onewire";

// DS2482-800 channel selection codes
// Based on DS2482-800 datasheet Table 2
// Write codes sent with channel select command (0xC3)
static const uint8_t CHANNEL_WRITE_CODES[8] = {
    0xF0,  // Channel 0 (IO0)
    0xE1,  // Channel 1 (IO1)
    0xD2,  // Channel 2 (IO2)
    0xC3,  // Channel 3 (IO3)
    0xB4,  // Channel 4 (IO4)
    0xA5,  // Channel 5 (IO5)
    0x96,  // Channel 6 (IO6)
    0x87   // Channel 7 (IO7)
};

// Read verification codes (expected response after channel select)
static const uint8_t CHANNEL_READ_CODES[8] = {
    0xB8,  // Channel 0 verification
    0xB1,  // Channel 1 verification
    0xAA,  // Channel 2 verification
    0xA3,  // Channel 3 verification
    0x9C,  // Channel 4 verification
    0x95,  // Channel 5 verification
    0x8E,  // Channel 6 verification
    0x87   // Channel 7 verification
};

void DS2482OneWireBus::dump_config() {
  ESP_LOGCONFIG(TAG, "DS2482 1-Wire Bus:");
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Active Pullup: %s", YESNO(this->active_pullup_));
  ESP_LOGCONFIG(TAG, "  Strong Pullup: %s", YESNO(this->strong_pullup_));
  this->dump_devices_(TAG);
}

bool DS2482OneWireBus::select_channel_() {
  // Optimization: only send channel select if different from current
  if (this->current_channel_ == this->channel_) {
    ESP_LOGVV(TAG, "Channel %d already selected", this->channel_);
    return true;
  }

  ESP_LOGVV(TAG, "Selecting channel %d", this->channel_);

  // Send channel select command (0xC3) + channel code
  uint8_t channel_select_cmd[2] = {0xC3, CHANNEL_WRITE_CODES[this->channel_]};

  if (this->write(channel_select_cmd, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to write channel select command");
    this->current_channel_ = 0xFF;  // Invalidate cache
    return false;
  }

  // Read back channel code to verify selection
  uint8_t response;
  if (this->read(&response, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read channel select response");
    this->current_channel_ = 0xFF;
    return false;
  }

  // Verify response matches expected read code
  if (response != CHANNEL_READ_CODES[this->channel_]) {
    ESP_LOGE(TAG, "Channel select verification failed: expected 0x%02X, got 0x%02X", CHANNEL_READ_CODES[this->channel_],
             response);
    this->current_channel_ = 0xFF;
    return false;
  }

  this->current_channel_ = this->channel_;
  ESP_LOGVV(TAG, "Channel %d selected successfully", this->channel_);
  return true;
}

bool DS2482OneWireBus::pre_operation_hook_() {
  // Called before every 1-Wire operation to ensure correct channel is selected
  return this->select_channel_();
}

void DS2482OneWireBus::post_reset_hook_() {
  // After device reset, channel selection is lost - invalidate cache
  ESP_LOGVV(TAG, "Device reset - invalidating channel cache");
  this->current_channel_ = 0xFF;
}

}  // namespace ds2482
}  // namespace esphome
