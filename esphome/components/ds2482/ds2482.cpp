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
  if (this->is_ds2482_800_) {
    ESP_LOGCONFIG(TAG, "  Variant: DS2482-800 (8-channel)");
    ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
  } else {
    ESP_LOGCONFIG(TAG, "  Variant: DS2482-100 (single-channel)");
  }
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Active Pullup: %s", YESNO(this->active_pullup_));
  ESP_LOGCONFIG(TAG, "  Strong Pullup: %s", YESNO(this->strong_pullup_));
  this->dump_devices_(TAG);
}

bool DS2482OneWireBus::detect_variant_() {
  // Try to select channel 7 - this only works on DS2482-800
  // DS2482-100 will reject this command
  ESP_LOGVV(TAG, "Detecting DS2482 variant...");

  uint8_t channel_select_cmd[2] = {0xC3, 0x87};  // Select channel 7
  if (this->write(channel_select_cmd, 2) != i2c::ERROR_OK) {
    ESP_LOGVV(TAG, "Channel select failed - likely DS2482-100");
    return false;  // DS2482-100
  }

  uint8_t response;
  if (this->read(&response, 1) != i2c::ERROR_OK) {
    ESP_LOGVV(TAG, "Read response failed - likely DS2482-100");
    return false;  // DS2482-100
  }

  // If we got the expected response (0x87), it's a DS2482-800
  if (response == 0x87) {
    ESP_LOGVV(TAG, "Got channel 7 response - DS2482-800 detected");
    return true;  // DS2482-800
  }

  ESP_LOGVV(TAG, "Unexpected response 0x%02X - likely DS2482-100", response);
  return false;  // DS2482-100
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

void DS2482OneWireBus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS2482 1-Wire bus...");

  // Reset device first
  if (!this->reset_device()) {
    ESP_LOGE(TAG, "Device reset failed");
    this->mark_failed();
    return;
  }

  // Detect whether we have an 800 (8-channel) or 100 (single-channel) variant
  this->is_ds2482_800_ = this->detect_variant_();

  if (this->is_ds2482_800_) {
    ESP_LOGCONFIG(TAG, "Detected DS2482-800 (8-channel)");
  } else {
    ESP_LOGCONFIG(TAG, "Detected DS2482-100 (single-channel)");
    if (this->channel_ != 0) {
      ESP_LOGW(TAG, "DS2482-100 only supports channel 0, ignoring configured channel %d", this->channel_);
      this->channel_ = 0;
    }
  }

  // Search for devices on this bus
  this->search();
}

bool DS2482OneWireBus::pre_operation_hook_() {
  // Only select channel if we have a DS2482-800 variant
  // DS2482-100 doesn't support channel selection
  if (this->is_ds2482_800_) {
    return this->select_channel_();
  }
  return true;  // No channel selection needed for DS2482-100
}

void DS2482OneWireBus::post_reset_hook_() {
  // After device reset, channel selection is lost - invalidate cache
  ESP_LOGVV(TAG, "Device reset - invalidating channel cache");
  this->current_channel_ = 0xFF;
}

}  // namespace ds2482
}  // namespace esphome
