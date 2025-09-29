#include "ds2484.h"

namespace esphome {
namespace ds2484 {
static const char *const TAG = "ds2484.onewire";

void DS2484OneWireBus::setup() {
  this->reset_device();
  this->search();
}

void DS2484OneWireBus::dump_config() {
  ESP_LOGCONFIG(TAG, "1-wire bus:");
  this->dump_devices_(TAG);
}

bool DS2484OneWireBus::read_status_(uint8_t *status) {
  for (uint8_t retry_nr = 0; retry_nr < 10; retry_nr++) {
    if (this->read(status, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "read status error");
      return false;
    }
    ESP_LOGVV(TAG, "status: %02x", *status);
    if (!(*status & 1)) {
      return true;
    }
  }
  ESP_LOGE(TAG, "read status error: too many retries");
  return false;
}

bool DS2484OneWireBus::wait_for_completion_() {
  uint8_t status;
  return this->read_status_(&status);
}

bool DS2484OneWireBus::reset_device() {
  ESP_LOGVV(TAG, "reset_device");
  uint8_t device_reset_cmd = 0xf0;
  uint8_t response;
  if (this->write(&device_reset_cmd, 1) != i2c::ERROR_OK) {
    return false;
  }
  if (!this->wait_for_completion_()) {
    ESP_LOGE(TAG, "reset_device: can't complete");
    return false;
  }
  uint8_t config = (this->active_pullup_ ? 1 : 0) | (this->strong_pullup_ ? 4 : 0);
  uint8_t write_config[2] = {0xd2, (uint8_t) (config | (~config << 4))};
  if (this->write(write_config, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "reset_device: can't write config");
    return false;
  }
  if (this->read(&response, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "can't read read8 response");
    return false;
  }
  if (response != (write_config[1] & 0xf)) {
    ESP_LOGE(TAG, "configuration didn't update");
    return false;
  }
  return true;
};

int DS2484OneWireBus::reset_int() {
  ESP_LOGVV(TAG, "reset");
  uint8_t reset_cmd = 0xb4;
  if (this->write(&reset_cmd, 1) != i2c::ERROR_OK) {
    return -1;
  }
  return this->wait_for_completion_() ? 1 : 0;
};

void DS2484OneWireBus::write8_(uint8_t value) {
  uint8_t buffer[2] = {0xa5, value};
  this->write(buffer, 2);
  this->wait_for_completion_();
};

void DS2484OneWireBus::write8(uint8_t value) {
  ESP_LOGVV(TAG, "write8: %02x", value);
  this->write8_(value);
};

void DS2484OneWireBus::write64(uint64_t value) {
  ESP_LOGVV(TAG, "write64: %llx", value);
  for (uint8_t i = 0; i < 8; i++) {
    this->write8_((value >> (i * 8)) & 0xff);
  }
}

uint8_t DS2484OneWireBus::read8() {
  uint8_t read8_cmd = 0x96;
  uint8_t set_read_reg_cmd[2] = {0xe1, 0xe1};
  uint8_t response = 0;
  if (this->write(&read8_cmd, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "can't write read8 cmd");
    return 0;
  }
  this->wait_for_completion_();
  if (this->write(set_read_reg_cmd, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "can't set read data reg");
    return 0;
  }
  if (this->read(&response, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "can't read read8 response");
    return 0;
  }
  return response;
}

uint64_t DS2484OneWireBus::read64() {
  uint8_t response = 0;
  for (uint8_t i = 0; i < 8; i++) {
    response |= (this->read8() << (i * 8));
  }
  return response;
}

void DS2484OneWireBus::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->address_ = 0;
}

bool DS2484OneWireBus::one_wire_triple_(bool *branch, bool *id_bit, bool *cmp_id_bit) {
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
  *id_bit = bool(status & 0x20);
  *cmp_id_bit = bool(status & 0x40);
  *branch = bool(status & 0x80);
  return true;
}

uint64_t IRAM_ATTR DS2484OneWireBus::search_int() {
  ESP_LOGVV(TAG, "search_int");
  if (this->last_device_flag_) {
    ESP_LOGVV(TAG, "last device flag set, quitting");
    return 0u;
  }

  uint8_t last_zero = 0;
  uint64_t bit_mask = 1;
  uint64_t address = this->address_;

  // Initiate search
  for (uint8_t bit_number = 1; bit_number <= 64; bit_number++, bit_mask <<= 1) {
    bool branch;

    // compute branch value for the case when there is a discrepancy
    // (there are devices with both 0s and 1s at this bit)
    if (bit_number < this->last_discrepancy_) {
      branch = (address & bit_mask) > 0;
    } else {
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

    if (!id_bit && !cmp_id_bit && !branch) {
      last_zero = bit_number;
    }

    ESP_LOGVV(TAG, "%d %d branch: %d %d", id_bit, cmp_id_bit, branch_before, branch);

    if (branch) {
      address |= bit_mask;
    } else {
      address &= ~bit_mask;
    }
  }
  ESP_LOGVV(TAG, "last_discepancy: %d", last_zero);
  ESP_LOGVV(TAG, "address: %llx", address);
  this->last_discrepancy_ = last_zero;
  if (this->last_discrepancy_ == 0) {
    // we're at root and have no choices left, so this was the last one.
    this->last_device_flag_ = true;
  }

  this->address_ = address;
  return address;
}

}  // namespace ds2484
}  // namespace esphome
