#include "ds248x_one_wire_bus.h"
#include "ds248x.h"
#include "esphome/core/log.h"

namespace esphome::ds248x {

static const char *const TAG = "ds248x.one_wire";

void DS248xOneWireBus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS248x 1-Wire Bus (Channel %d)...", this->channel_);

  // Parent setup happens in DS248xComponent::setup()
  // We just need to scan for devices on this channel
  if (!this->ensure_channel_()) {
    ESP_LOGE(TAG, "Failed to select channel %d during setup", this->channel_);
    this->mark_failed();
    return;
  }

  // Perform device search on this channel
  this->search();

  ESP_LOGCONFIG(TAG, "Found %zu devices on channel %d", this->devices_.size(), this->channel_);
}

void DS248xOneWireBus::dump_config() {
  ESP_LOGCONFIG(TAG, "DS248x 1-Wire Bus (Channel %d):", this->channel_);
  this->dump_devices_(TAG);
}

bool DS248xOneWireBus::ensure_channel_() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set!");
    return false;
  }
  return this->parent_->select_channel(this->channel_);
}

int DS248xOneWireBus::reset_int() {
  if (!this->ensure_channel_()) {
    return -1;
  }

  bool presence = false;
  if (!this->parent_->ow_reset(presence)) {
    return -1;
  }
  return presence ? 1 : 0;
}

void DS248xOneWireBus::write8(uint8_t val) {
  if (!this->ensure_channel_()) {
    return;
  }
  if (!this->parent_->ow_write_byte(val)) {
    ESP_LOGE(TAG, "Failed to write byte 0x%02X on channel %d", val, this->channel_);
  }
}

void DS248xOneWireBus::write64(uint64_t val) {
  if (!this->ensure_channel_()) {
    return;
  }
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t byte = static_cast<uint8_t>(val >> (i * 8));
    if (!this->parent_->ow_write_byte(byte)) {
      ESP_LOGE(TAG, "Failed to write byte %d/8 (0x%02X) on channel %d - aborting write64", i + 1, byte, this->channel_);
      return;  // Stop writing to prevent sending corrupted data
    }
  }
}

uint8_t DS248xOneWireBus::read8() {
  if (!this->ensure_channel_()) {
    return 0;
  }
  uint8_t value = 0;
  if (!this->parent_->ow_read_byte(value)) {
    ESP_LOGE(TAG, "Failed to read byte on channel %d", this->channel_);
  }
  return value;
}

uint64_t DS248xOneWireBus::read64() {
  if (!this->ensure_channel_()) {
    return 0;
  }
  uint64_t value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t byte = 0;
    if (!this->parent_->ow_read_byte(byte)) {
      ESP_LOGE(TAG, "Failed to read byte %d/8 on channel %d - returning partial data", i + 1, this->channel_);
      return value;  // Return partial data to avoid blocking, caller should validate
    }
    value |= (static_cast<uint64_t>(byte) << (i * 8));
  }
  return value;
}

void DS248xOneWireBus::reset_search() {
  this->search_last_discrepancy_ = 0;
  this->search_last_device_flag_ = false;
  this->search_address_ = 0;
}

uint64_t DS248xOneWireBus::search_int() {
  if (!this->ensure_channel_()) {
    return 0;
  }

  if (this->search_last_device_flag_) {
    return 0;
  }

  uint8_t last_zero = 0;
  uint64_t address = this->search_address_;

  // Iterate through all 64 bits
  for (uint8_t bit_number = 1; bit_number <= 64; bit_number++) {
    uint64_t bit_mask = 1ULL << (bit_number - 1);

    // Determine search direction
    bool search_direction;
    if (bit_number < this->search_last_discrepancy_) {
      search_direction = (address & bit_mask) != 0;
    } else {
      search_direction = (bit_number == this->search_last_discrepancy_);
    }

    // Perform triplet operation
    uint8_t status = 0;
    if (!this->parent_->search_triplet(search_direction, status)) {
      ESP_LOGW(TAG, "1-Wire triplet failed at bit %d on channel %d - aborting search", bit_number, this->channel_);
      this->reset_search();
      return 0;
    }

    bool id_bit = (status & DS248X_STATUS_SBR) != 0;
    bool cmp_id_bit = (status & DS248X_STATUS_TSB) != 0;
    bool dir_taken = (status & DS248X_STATUS_DIR) != 0;

    if (id_bit && cmp_id_bit) {
      // No devices participating
      this->reset_search();
      return 0;
    }

    if (!id_bit && !cmp_id_bit && !dir_taken) {
      // Discrepancy, went 0 - record position
      last_zero = bit_number;
    }

    // Update address based on direction taken
    if (dir_taken) {
      address |= bit_mask;
    } else {
      address &= ~bit_mask;
    }
  }

  // Search successful
  this->search_last_discrepancy_ = last_zero;
  if (last_zero == 0) {
    this->search_last_device_flag_ = true;
  }
  this->search_address_ = address;

  return address;
}

}  // namespace esphome::ds248x
