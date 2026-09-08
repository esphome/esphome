#include "ds248x.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome::ds248x {

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
  do {
    uint8_t status;
    if (this->read(&status, 1) == i2c::ERROR_OK && !(status & DS248X_STATUS_BUSY))
      return true;
    delayMicroseconds(100);
  } while (millis() - start < BUSY_TIMEOUT_MS);
  ESP_LOGW(TAG, "DS248x busy timeout");
  bool recovered = this->device_reset_() && this->device_configure_();
  this->current_channel_ = -1;
  if (!recovered) {
    ESP_LOGE(TAG, "DS248x recovery failed after busy timeout");
    this->mark_failed();
  }
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
  return true;
}

bool DS248xComponent::device_configure_() {
  ESP_LOGD(TAG, "Configuring device...");

  if (!this->write_config_()) {
    ESP_LOGW(TAG, "Config write/verify failed");
    return false;
  }

  ESP_LOGD(TAG, "Configured successfully");

  // DS2484 Configuration
  if (this->ds2484_mode_) {
    if (this->ds2484_trstl_ != DS2484_PARAM_UNSET &&
        !this->configure_ds2484_port_(DS2484_PORT_PARAM_TRSTL, this->ds2484_trstl_))
      return false;
    if (this->ds2484_tmsp_ != DS2484_PARAM_UNSET &&
        !this->configure_ds2484_port_(DS2484_PORT_PARAM_TMSP, this->ds2484_tmsp_))
      return false;
    if (this->ds2484_tw0l_ != DS2484_PARAM_UNSET &&
        !this->configure_ds2484_port_(DS2484_PORT_PARAM_TW0L, this->ds2484_tw0l_))
      return false;
    if (this->ds2484_trec0_ != DS2484_PARAM_UNSET &&
        !this->configure_ds2484_port_(DS2484_PORT_PARAM_TREC0, this->ds2484_trec0_))
      return false;
    if (this->ds2484_rwpu_ != DS2484_PARAM_UNSET &&
        !this->configure_ds2484_port_(DS2484_PORT_PARAM_RWPU, this->ds2484_rwpu_))
      return false;
  }

  return true;
}

bool DS248xComponent::configure_ds2484_port_(uint8_t param, uint8_t val) {
  uint8_t cmd = DS2484_COMMAND_ADJUSTPORT;
  // Control Byte format (DS2484 Table 6): P[2:0] in bits 7:5, OD in bit 4, VAL[3:0] in bits 3:0
  uint8_t data = ((param & 0x07) << 5) | (val & 0x0F);

  // The DS2484 always acknowledges the Adjust 1-Wire Port control byte (datasheet "Adjust
  // 1-Wire Port"), so a successful write confirms the update. We deliberately do not read
  // back to verify: a single read of the Port Configuration register always returns the
  // fixed 8-byte report starting at Byte 1 (tRSTL standard speed), not the parameter that
  // was just written, so a per-parameter readback comparison would spuriously fail for
  // tMSP/tW0L/tREC0/RWPU.
  if (!this->write_byte(cmd, data)) {
    ESP_LOGW(TAG, "DS2484 port config failed (param %d)", param);
    return false;
  }

  return this->set_read_pointer_(DS248X_POINTER_STATUS);
}

bool DS248xComponent::write_config_() {
  uint8_t config = 0;
  if (this->active_pullup_)
    config |= DS248X_CONFIG_ACTIVE_PULLUP;

  // The DS248x only accepts the config byte if the upper nibble is the one's-complement of the lower nibble.
  uint8_t config_byte = (config & 0x0F) | ((~config & 0x0F) << 4);

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

bool DS248xComponent::ow_write_byte(uint8_t byte) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

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

bool DS248xComponent::search_triplet(bool search_direction, uint8_t &status) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  // DS248x Datasheet: 1-Wire Triplet command requires 2 bytes:
  // Byte 1: Command code 0x78
  // Byte 2: Direction byte (bit 7 = V, search direction if discrepancy)
  uint8_t buffer[2] = {DS248X_COMMAND_TRIPLET, static_cast<uint8_t>(search_direction ? 0x80 : 0x00)};
  if (this->write(buffer, 2) != i2c::ERROR_OK)
    return false;

  if (!this->wait_busy_())
    return false;

  if (this->read(&status, 1) != i2c::ERROR_OK)
    return false;

  return true;
}

}  // namespace esphome::ds248x
