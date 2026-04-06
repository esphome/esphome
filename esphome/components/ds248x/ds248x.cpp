#include "ds248x.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ds248x {

static const char *const TAG = "ds248x";

void DS248xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS248x...");

  // Wake up device if sleep pin is configured
  if (this->sleep_pin_) {
    this->sleep_pin_->setup();
    this->sleep_pin_->pin_mode(esphome::gpio::FLAG_OUTPUT);
    this->sleep_pin_->digital_write(true);  // Wake up
    delay(1);                               // DS2482-101 Datasheet: tOSCWUP = 100μs (using 10x margin)
  }

  // Probe device
  ESP_LOGD(TAG, "Probing DS248x...");
  uint8_t status = 0;
  if (this->read(&status, 1) == i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Device responded! Status: 0x%02x", status);
  } else {
    ESP_LOGW(TAG, "Device did not respond. Trying reset anyway...");
  }

  if (!this->device_reset_()) {
    ESP_LOGW(TAG, "DS248x reset failed during setup!");
  }

  // Configure device
  if (!this->device_configure_()) {
    ESP_LOGE(TAG, "DS248x configuration failed!");
    this->mark_failed();
    return;
  }

  // Reset to Channel 0
  this->select_channel(0);

  ESP_LOGI(TAG, "DS248x initialized successfully.");
}

void DS248xComponent::on_shutdown() {
  if (this->sleep_pin_ && (this->hub_sleep_ || this->bus_sleep_)) {
    this->sleep_pin_->digital_write(false);  // Sleep
  }
}

void DS248xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DS248x:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Channel Count: %d", this->channel_count_);
  ESP_LOGCONFIG(TAG, "  Active Pullup: %s", YESNO(this->active_pullup_));
  ESP_LOGCONFIG(TAG, "  Strong Pullup: %s", YESNO(this->strong_pullup_enabled_));
  if (this->ds2484_mode_) {
    ESP_LOGCONFIG(TAG, "  DS2484 Mode: enabled");
  }
}

// --- Internal Helpers ---

// Datasheet command durations are sub-2ms; allow a little margin before forcing recovery.
static constexpr uint32_t BUSY_TIMEOUT_MS = 5;

bool DS248xComponent::set_read_pointer_(uint8_t ptr) { return this->write_byte(DS248X_COMMAND_SETREADPTR, ptr); }

bool DS248xComponent::wait_busy_() {
  uint32_t start = millis();
  while (millis() - start < BUSY_TIMEOUT_MS) {
    uint8_t status;
    if (this->read(&status, 1) != i2c::ERROR_OK) {
      delayMicroseconds(100);
      continue;
    }
    if (!(status & DS248X_STATUS_BUSY))
      return true;
    delayMicroseconds(100);
  }
  ESP_LOGW(TAG, "DS248x busy timeout");
  this->device_reset_();
  this->current_channel_ = -1;
  return false;
}

bool DS248xComponent::device_reset_() {
  ESP_LOGD(TAG, "Resetting device...");
  uint8_t cmd = DS248X_COMMAND_RESET;
  if (this->write(&cmd, 1) != i2c::ERROR_OK)
    return false;

  uint8_t status;
  if (this->read(&status, 1) != i2c::ERROR_OK)
    return false;

  if (!(status & DS248X_STATUS_RST)) {
    ESP_LOGW(TAG, "Device reset failed (RST bit not set)");
    return false;
  }

  this->current_channel_ = -1;
  this->last_config_byte_ = 0xFF;
  this->strong_pullup_active_ = false;
  return true;
}

bool DS248xComponent::device_configure_() {
  ESP_LOGD(TAG, "Configuring device...");

  if (!this->set_strong_pullup_mode_(false)) {
    ESP_LOGW(TAG, "Config write/verify failed");
    return false;
  }

  ESP_LOGD(TAG, "Configured successfully");

  // DS2484 Configuration
  if (this->ds2484_mode_) {
    if (this->ds2484_trstl_ > 0)
      this->configure_ds2484_port_(0x0, this->ds2484_trstl_);
    if (this->ds2484_tmsp_ > 0)
      this->configure_ds2484_port_(0x1, this->ds2484_tmsp_);
    if (this->ds2484_tw0l_ > 0)
      this->configure_ds2484_port_(0x2, this->ds2484_tw0l_);
    if (this->ds2484_trec0_ > 0)
      this->configure_ds2484_port_(0x3, this->ds2484_trec0_);
    if (this->ds2484_rwpu_ > 0)
      this->configure_ds2484_port_(0x4, this->ds2484_rwpu_);
  }

  return true;
}

bool DS248xComponent::configure_ds2484_port_(uint8_t param, uint8_t val) {
  uint8_t cmd = 0xC3;
  // Control Byte format (DS2484 Table 6): P[2:0] in bits 7:5, OD in bit 4, VAL[3:0] in bits 3:0
  uint8_t data = ((param & 0x07) << 5) | (val & 0x0F);

  if (!this->write_byte(cmd, data)) {
    ESP_LOGW(TAG, "DS2484 port config failed (param %d)", param);
    return false;
  }

  return this->set_read_pointer_(DS248X_POINTER_STATUS);
}

bool DS248xComponent::set_strong_pullup_mode_(bool enable) {
  uint8_t config = 0;
  if (this->active_pullup_ || enable)
    config |= DS248X_CONFIG_ACTIVE_PULLUP;
  if (this->overdrive_speed_)
    config |= DS248X_CONFIG_OVERDRIVE;
  // Match the legacy ds2484 backend: when configured, keep SPU armed across commands.
  if (this->strong_pullup_enabled_ || enable)
    config |= DS248X_CONFIG_STRONG_PULLUP;

  uint8_t config_byte = (config & 0x0F) | ((~config & 0x0F) << 4);
  if (config_byte == this->last_config_byte_) {
    this->strong_pullup_active_ = enable;
    return this->set_read_pointer_(DS248X_POINTER_STATUS);
  }

  if (!this->write_byte(DS248X_COMMAND_WRITECONFIG, config_byte)) {
    ESP_LOGW(TAG, "Failed to write config byte");
    return false;
  }

  if (!this->set_read_pointer_(DS248X_POINTER_CONFIG)) {
    return false;
  }

  uint8_t read_config;
  if (this->read(&read_config, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read back config byte");
    return false;
  }

  if ((read_config & 0x0F) != (config_byte & 0x0F)) {
    ESP_LOGW(TAG, "Config mismatch! Wrote 0x%02x, Read 0x%02x", config_byte, read_config);
    return false;
  }

  this->last_config_byte_ = config_byte;
  this->strong_pullup_active_ = enable;
  return this->set_read_pointer_(DS248X_POINTER_STATUS);
}

// --- Channel Selection ---

// Channel select codes: write code -> expected read code
static constexpr uint8_t CHANNEL_WRITE_CODES[8] = {0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87};
static constexpr uint8_t CHANNEL_READ_CODES[8] = {0xB8, 0xB1, 0xAA, 0xA3, 0x9C, 0x95, 0x8E, 0x87};

bool DS248xComponent::select_channel(uint8_t channel) {
  if (this->channel_count_ <= 1)
    return true;
  if (channel >= this->channel_count_)
    return false;

  if (this->current_channel_ == channel)
    return true;

  if (!this->write_byte(DS248X_COMMAND_CHANNELSELECT, CHANNEL_WRITE_CODES[channel])) {
    this->current_channel_ = -1;
    return false;
  }

  uint8_t read_code;
  if (this->read(&read_code, 1) != i2c::ERROR_OK) {
    this->current_channel_ = -1;
    return false;
  }

  if (read_code != CHANNEL_READ_CODES[channel]) {
    ESP_LOGW(TAG, "Channel select failed! Expected 0x%02x, got 0x%02x", CHANNEL_READ_CODES[channel], read_code);
    this->current_channel_ = -1;
    return false;
  }

  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  this->current_channel_ = channel;
  return true;
}

// --- 1-Wire Bus Operations ---

bool DS248xComponent::ow_reset(bool &presence) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  uint8_t cmd = DS248X_COMMAND_RESETWIRE;
  if (this->write(&cmd, 1) != i2c::ERROR_OK)
    return false;

  if (!this->wait_busy_()) {
    ESP_LOGW(TAG, "ow_reset: wait busy failed");
    return false;
  }

  uint8_t status;
  if (this->read(&status, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "ow_reset: read status failed");
    return false;
  }

  if (status & DS248X_STATUS_SD) {
    ESP_LOGW(TAG, "Short detected on 1-Wire bus!");
    return false;
  }

  presence = (status & DS248X_STATUS_PPD);
  return true;
}

bool DS248xComponent::ow_write_byte(uint8_t byte, bool keep_strong_pullup) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  if (keep_strong_pullup) {
    if (!this->set_strong_pullup_mode_(true)) {
      ESP_LOGW(TAG, "Failed to arm strong pullup for byte 0x%02x", byte);
      return false;
    }
  }

  if (!this->wait_busy_()) {
    ESP_LOGW(TAG, "Device busy before writing byte 0x%02x", byte);
    return false;
  }

  uint8_t cmd[2] = {DS248X_COMMAND_WRITEBYTE, byte};
  if (this->write(cmd, 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write failed for byte 0x%02x", byte);
    return false;
  }

  if (!this->wait_busy_()) {
    ESP_LOGW(TAG, "Timeout waiting for write byte to complete!");
    return false;
  }

  if (!keep_strong_pullup && this->strong_pullup_active_) {
    this->set_strong_pullup_mode_(false);
  }

  return true;
}

bool DS248xComponent::ow_read_byte(uint8_t &byte) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  uint8_t cmd = DS248X_COMMAND_READBYTE;
  if (this->write(&cmd, 1) != i2c::ERROR_OK)
    return false;

  if (!this->wait_busy_())
    return false;

  if (!this->set_read_pointer_(DS248X_POINTER_DATA))
    return false;

  if (this->read(&byte, 1) != i2c::ERROR_OK)
    return false;

  return true;
}

uint8_t DS248xComponent::search_triplet(bool search_direction) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return 0;

  // DS248x Datasheet: 1-Wire Triplet command requires 2 bytes:
  // Byte 1: Command code 0x78
  // Byte 2: Direction byte (bit 7 = V, search direction if discrepancy)
  uint8_t buffer[2] = {DS248X_COMMAND_TRIPLET, static_cast<uint8_t>(search_direction ? 0x80 : 0x00)};
  if (this->write(buffer, 2) != i2c::ERROR_OK)
    return 0;

  if (!this->wait_busy_())
    return 0;

  uint8_t status;
  if (this->read(&status, 1) != i2c::ERROR_OK)
    return 0;

  return status;
}

}  // namespace ds248x
}  // namespace esphome
