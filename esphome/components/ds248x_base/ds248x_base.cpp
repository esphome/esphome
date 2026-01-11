#include "ds248x_base.h"

namespace esphome {
namespace ds248x_base {
static const char *const TAG = "ds248x.onewire";

void DS248xOneWireBusBase::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS248x 1-Wire bus...");
  if (!this->reset_device()) {
    ESP_LOGE(TAG, "Device reset failed");
    this->mark_failed();
    return;
  }
  this->search();
}

void DS248xOneWireBusBase::dump_config() {
  ESP_LOGCONFIG(TAG, "DS248x 1-Wire Bus:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Active Pullup: %s", YESNO(this->active_pullup_));
  ESP_LOGCONFIG(TAG, "  Strong Pullup: %s", YESNO(this->strong_pullup_));
  this->dump_devices_(TAG);
}

bool DS248xOneWireBusBase::read_status_(uint8_t *status) {
  for (uint8_t retry_nr = 0; retry_nr < 10; retry_nr++) {
    if (this->read(status, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "read status error");
      return false;
    }
    ESP_LOGVV(TAG, "status: %02x", *status);
    // Check busy bit (bit 0)
    if (!(*status & 1)) {
      return true;
    }
  }
  ESP_LOGE(TAG, "read status error: too many retries");
  return false;
}

bool DS248xOneWireBusBase::wait_for_completion_() {
  uint8_t status;
  return this->read_status_(&status);
}

bool DS248xOneWireBusBase::reset_device() {
  ESP_LOGVV(TAG, "reset_device");

  // Send device reset command (0xF0)
  uint8_t device_reset_cmd = 0xf0;
  if (this->write(&device_reset_cmd, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "reset_device: can't write reset command");
    return false;
  }

  if (!this->wait_for_completion_()) {
    ESP_LOGE(TAG, "reset_device: can't complete");
    return false;
  }

  // Call subclass hook (DS2482 invalidates channel cache here)
  this->post_reset_hook_();

  // Write configuration register
  // Bit 0: Active Pullup (APU) - 1.5kΩ pullup
  // Bit 2: Strong Pullup (SPU) - for power delivery
  // Upper nibble: complement of lower nibble (verification)
  uint8_t config = (this->active_pullup_ ? 1 : 0) | (this->strong_pullup_ ? 4 : 0);
  uint8_t write_config[2] = {0xd2, (uint8_t) (config | (~config << 4))};

  if (this->write(write_config, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "reset_device: can't write config");
    return false;
  }

  // Read back and verify configuration
  uint8_t response;
  if (this->read(&response, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "reset_device: can't read config response");
    return false;
  }

  if (response != (write_config[1] & 0xf)) {
    ESP_LOGE(TAG, "configuration didn't update (expected 0x%02x, got 0x%02x)", write_config[1] & 0xf, response);
    return false;
  }

  return true;
}

int DS248xOneWireBusBase::reset_int() {
  ESP_LOGVV(TAG, "reset");

  // Call subclass hook (DS2482 selects channel here)
  if (!this->pre_operation_hook_()) {
    return -1;
  }

  // Send 1-Wire reset command (0xB4)
  uint8_t reset_cmd = 0xb4;
  if (this->write(&reset_cmd, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "reset: write failed");
    return -1;
  }

  return this->wait_for_completion_() ? 1 : 0;
}

void DS248xOneWireBusBase::write8_(uint8_t value) {
  // Send 1-Wire write byte command (0xA5) + data
  uint8_t buffer[2] = {0xa5, value};
  this->write(buffer, 2);
  this->wait_for_completion_();
}

void DS248xOneWireBusBase::write8(uint8_t value) {
  ESP_LOGVV(TAG, "write8: %02x", value);

  // Call subclass hook (DS2482 selects channel here)
  if (!this->pre_operation_hook_()) {
    ESP_LOGE(TAG, "write8: pre_operation_hook failed");
    return;
  }

  this->write8_(value);
}

void DS248xOneWireBusBase::write64(uint64_t value) {
  ESP_LOGVV(TAG, "write64: %llx", value);

  // Call subclass hook once (DS2482 selects channel here)
  if (!this->pre_operation_hook_()) {
    ESP_LOGE(TAG, "write64: pre_operation_hook failed");
    return;
  }

  // Write 8 bytes, LSB first
  for (uint8_t i = 0; i < 8; i++) {
    this->write8_((value >> (i * 8)) & 0xff);
  }
}

uint8_t DS248xOneWireBusBase::read8() {
  // Call subclass hook (DS2482 selects channel here)
  if (!this->pre_operation_hook_()) {
    ESP_LOGE(TAG, "read8: pre_operation_hook failed");
    return 0;
  }

  // Send 1-Wire read byte command (0x96)
  uint8_t read8_cmd = 0x96;
  if (this->write(&read8_cmd, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "can't write read8 cmd");
    return 0;
  }
  this->wait_for_completion_();

  // Set read pointer to read data register (0xE1, 0xE1)
  uint8_t set_read_reg_cmd[2] = {0xe1, 0xe1};
  if (this->write(set_read_reg_cmd, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "can't set read data reg");
    return 0;
  }

  // Read the data byte
  uint8_t response = 0;
  if (this->read(&response, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "can't read read8 response");
    return 0;
  }

  ESP_LOGVV(TAG, "read8: %02x", response);
  return response;
}

uint64_t DS248xOneWireBusBase::read64() {
  ESP_LOGVV(TAG, "read64");

  // Read 8 bytes, LSB first
  uint64_t response = 0;
  for (uint8_t i = 0; i < 8; i++) {
    response |= ((uint64_t) this->read8() << (i * 8));
  }

  ESP_LOGVV(TAG, "read64: %llx", response);
  return response;
}

void DS248xOneWireBusBase::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->address_ = 0;
}

bool DS248xOneWireBusBase::one_wire_triple_(bool *branch, bool *id_bit, bool *cmp_id_bit) {
  // Send 1-Wire triplet command (0x78) with branch direction
  uint8_t buffer[2] = {(uint8_t) 0x78, (uint8_t) (*branch ? 0x80u : 0)};
  uint8_t status;

  if (!this->read_status_(&status)) {
    ESP_LOGE(TAG, "one_wire_triple start: read status error");
    return false;
  }

  if (this->write(buffer, 2) != i2c::ERROR_OK) {
    ESP_LOGV(TAG, "one_wire_triple: can't write cmd");
    return false;
  }

  if (!this->read_status_(&status)) {
    ESP_LOGE(TAG, "one_wire_triple: read status error");
    return false;
  }

  // Extract triplet results from status register
  *id_bit = bool(status & 0x20);      // ID bit
  *cmp_id_bit = bool(status & 0x40);  // Complement ID bit
  *branch = bool(status & 0x80);      // Branch direction taken

  return true;
}

uint64_t IRAM_ATTR DS248xOneWireBusBase::search_int() {
  ESP_LOGVV(TAG, "search_int");

  if (this->last_device_flag_) {
    ESP_LOGVV(TAG, "last device flag set, quitting");
    return 0u;
  }

  // Call subclass hook (DS2482 selects channel here)
  if (!this->pre_operation_hook_()) {
    ESP_LOGW(TAG, "search_int: pre_operation_hook failed");
    return 0;
  }

  uint8_t last_zero = 0;
  uint64_t bit_mask = 1;
  uint64_t address = this->address_;

  // Standard 1-Wire ROM search algorithm
  // Iterate through all 64 bits of the ROM code
  for (uint8_t bit_number = 1; bit_number <= 64; bit_number++, bit_mask <<= 1) {
    bool branch;

    // Compute branch value for discrepancy case
    // (when devices have both 0s and 1s at this bit position)
    if (bit_number < this->last_discrepancy_) {
      // Use previous path
      branch = (address & bit_mask) > 0;
    } else {
      // At or past last discrepancy - take 1 path if at discrepancy, else 0
      branch = bit_number == this->last_discrepancy_;
    }

    bool id_bit, cmp_id_bit;
    bool branch_before = branch;
    if (!this->one_wire_triple_(&branch, &id_bit, &cmp_id_bit)) {
      ESP_LOGW(TAG, "one wire triple error, quitting");
      return 0;
    }

    if (id_bit && cmp_id_bit) {
      ESP_LOGW(TAG, "no devices on the bus, quitting");
      // No devices participating in search
      return 0;
    }

    // If there was a discrepancy (both 0 and 1) and we took the 0 path,
    // remember this position for next search
    if (!id_bit && !cmp_id_bit && !branch) {
      last_zero = bit_number;
    }

    ESP_LOGVV(TAG, "%d %d branch: %d %d", id_bit, cmp_id_bit, branch_before, branch);

    // Update address based on branch taken
    if (branch) {
      address |= bit_mask;
    } else {
      address &= ~bit_mask;
    }
  }

  ESP_LOGVV(TAG, "last_discrepancy: %d", last_zero);
  ESP_LOGVV(TAG, "address: %llx", address);

  this->last_discrepancy_ = last_zero;
  if (this->last_discrepancy_ == 0) {
    // We're at root and have no choices left, so this was the last one
    this->last_device_flag_ = true;
  }

  this->address_ = address;
  return address;
}

}  // namespace ds248x_base
}  // namespace esphome
