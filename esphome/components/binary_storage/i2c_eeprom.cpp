#include "i2c_eeprom.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cstring>
#include <cctype>

namespace esphome {
namespace binary_storage {

static const char *const TAG = "i2c_eeprom";

void I2CEeprom::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2C EEPROM...");

  // Auto-configure from model string if capacity not set
  if (this->capacity_ == 0) {
    this->auto_configure_from_model_();
  }

  // Verify device is present
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Device not found at address 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // Call base class setup
  BinaryStorage::setup();

  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  Addressing: %u-bit", this->addressing_bits_);
}

void I2CEeprom::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C EEPROM:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  Capacity: %u bytes (%.1f KB)", this->capacity_, this->capacity_ / 1024.0f);
  ESP_LOGCONFIG(TAG, "  Page Size: %u bytes", this->page_size_);
  ESP_LOGCONFIG(TAG, "  Addressing: %u-bit", this->addressing_bits_);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
  }
}

void I2CEeprom::auto_configure_from_model_() {
  // Extract capacity from model string (e.g., "AT24C256" → 256 Kbit)
  std::string upper_model = this->model_;
  for (char &c : upper_model) {
    c = std::toupper(c);
  }

  // Common patterns: AT24Cxx, 24LCxx, 24AAxx, 24FCxx
  size_t pos = upper_model.find("24C");
  if (pos == std::string::npos)
    pos = upper_model.find("24LC");
  if (pos == std::string::npos)
    pos = upper_model.find("24AA");
  if (pos == std::string::npos)
    pos = upper_model.find("24FC");

  if (pos != std::string::npos) {
    // Find the number after "24C" or "24LC" etc.
    size_t num_start = upper_model.find_first_of("0123456789", pos);
    if (num_start != std::string::npos) {
      size_t num_end = upper_model.find_first_not_of("0123456789", num_start);
      std::string num_str = upper_model.substr(num_start, num_end - num_start);
      int capacity_kbit = std::stoi(num_str);

      // Convert Kbit to bytes
      this->capacity_ = (capacity_kbit * 1024) / 8;

      // Configure page size and addressing based on capacity
      if (capacity_kbit <= 2) {
        // 24C01, 24C02: 8-bit addressing, 8-byte pages
        this->addressing_bits_ = 8;
        this->page_size_ = 8;
      } else if (capacity_kbit == 4) {
        // 24C04: 9-bit addressing, 16-byte pages
        this->addressing_bits_ = 9;
        this->page_size_ = 16;
      } else if (capacity_kbit == 8) {
        // 24C08: 10-bit addressing, 16-byte pages
        this->addressing_bits_ = 10;
        this->page_size_ = 16;
      } else if (capacity_kbit == 16) {
        // 24C16: 11-bit addressing, 16-byte pages
        this->addressing_bits_ = 11;
        this->page_size_ = 16;
      } else if (capacity_kbit <= 64) {
        // 24C32, 24C64: 16-bit addressing, 32-byte pages
        this->addressing_bits_ = 16;
        this->page_size_ = 32;
      } else if (capacity_kbit <= 256) {
        // 24C128, 24C256: 16-bit addressing, 64-byte pages
        this->addressing_bits_ = 16;
        this->page_size_ = 64;
      } else {
        // 24C512+: 16-bit addressing, 128-byte pages
        this->addressing_bits_ = 16;
        this->page_size_ = 128;
      }

      ESP_LOGD(TAG, "Auto-configured: %u Kbit = %u bytes, page=%u, addr=%u-bit", capacity_kbit, this->capacity_,
               this->page_size_, this->addressing_bits_);
    }
  }

  if (this->capacity_ == 0) {
    ESP_LOGW(TAG, "Could not auto-configure from model '%s', using defaults", this->model_.c_str());
    this->capacity_ = 32 * 1024;  // Default 32KB
    this->page_size_ = 64;
    this->addressing_bits_ = 16;
  }
}

bool I2CEeprom::is_ready() {
  // Try to read from device to check if it's present and ready
  i2c::ErrorCode err = this->bus_->writev(this->address_, nullptr, 0);
  return err == i2c::ERROR_OK;
}

uint8_t I2CEeprom::get_device_address_for_memory_(uint32_t mem_address) const {
  uint8_t device_addr = this->address_;

  // For 9/10/11-bit addressing, some memory address bits go into I2C device address
  if (this->addressing_bits_ == 9) {
    // Bit 8 of memory address goes into A0 of device address
    device_addr |= (mem_address >> 8) & 0x01;
  } else if (this->addressing_bits_ == 10) {
    // Bits 8-9 of memory address go into A0-A1 of device address
    device_addr |= (mem_address >> 8) & 0x03;
  } else if (this->addressing_bits_ == 11) {
    // Bits 8-10 of memory address go into A0-A2 of device address
    device_addr |= (mem_address >> 8) & 0x07;
  }

  return device_addr;
}

bool I2CEeprom::wait_for_write_complete_(uint32_t timeout_ms) {
  uint32_t start = millis();

  // ACK polling: device will NACK while busy
  while (millis() - start < timeout_ms) {
    if (this->is_ready()) {
      return true;
    }
    delayMicroseconds(100);
  }

  ESP_LOGW(TAG, "Write timeout after %u ms", timeout_ms);
  return false;
}

bool I2CEeprom::read_block_(uint32_t address, uint8_t *data, size_t length) {
  uint8_t device_addr = this->get_device_address_for_memory_(address);

  i2c::ErrorCode err;

  if (this->addressing_bits_ <= 8) {
    // 8-bit addressing: send single address byte
    uint8_t addr_byte = address & 0xFF;
    err = this->bus_->write_readv(device_addr, &addr_byte, 1, data, length);
  } else {
    // 16-bit addressing: send two address bytes (MSB first)
    uint8_t addr_bytes[2] = {(uint8_t) ((address >> 8) & 0xFF), (uint8_t) (address & 0xFF)};
    err = this->bus_->write_readv(device_addr, addr_bytes, 2, data, length);
  }

  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Read failed at address 0x%04X: %d", address, err);
    return false;
  }

  return true;
}

bool I2CEeprom::read(uint32_t address, uint8_t *data, size_t length) {
  if (!this->is_valid_address_(address, length)) {
    ESP_LOGE(TAG, "Read address out of bounds: 0x%X + %u", address, length);
    return false;
  }

  // EEPROM can be read in one shot (unlike writes)
  // But we'll chunk it for reliability
  const size_t chunk_size = 32;

  while (length > 0) {
    size_t read_len = std::min(length, chunk_size);

    if (!this->read_block_(address, data, read_len)) {
      return false;
    }

    address += read_len;
    data += read_len;
    length -= read_len;
  }

  return true;
}

bool I2CEeprom::write_page_(uint32_t address, const uint8_t *data, size_t length) {
  if (length == 0 || length > this->page_size_) {
    ESP_LOGE(TAG, "Invalid page write length: %u (max %u)", length, this->page_size_);
    return false;
  }

  uint8_t device_addr = this->get_device_address_for_memory_(address);

  // Build write buffer: [address_bytes] + [data]
  uint8_t write_buffer[2 + 128];  // Max: 2 addr bytes + 128 data bytes
  size_t write_len = 0;

  if (this->addressing_bits_ <= 8) {
    // 8-bit addressing
    write_buffer[write_len++] = address & 0xFF;
  } else {
    // 16-bit addressing
    write_buffer[write_len++] = (address >> 8) & 0xFF;
    write_buffer[write_len++] = address & 0xFF;
  }

  // Copy data
  memcpy(&write_buffer[write_len], data, length);
  write_len += length;

  // Write to device
  i2c::ErrorCode err = this->bus_->writev(device_addr, write_buffer, write_len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Page write failed at address 0x%04X: %d", address, err);
    return false;
  }

  // Wait for write to complete
  return this->wait_for_write_complete_();
}

bool I2CEeprom::write(uint32_t address, const uint8_t *data, size_t length) {
  if (!this->is_valid_address_(address, length)) {
    ESP_LOGE(TAG, "Write address out of bounds: 0x%X + %u", address, length);
    return false;
  }

  // EEPROM requires page-aligned writes
  while (length > 0) {
    // Calculate how many bytes we can write without crossing page boundary
    uint32_t bytes_to_boundary = this->get_bytes_to_page_boundary_(address);
    size_t write_len = std::min((size_t) bytes_to_boundary, length);

    if (!this->write_page_(address, data, write_len)) {
      return false;
    }

    address += write_len;
    data += write_len;
    length -= write_len;
  }

  return true;
}

bool I2CEeprom::sync() {
  // Just ensure device is ready (any pending write is complete)
  return this->wait_for_write_complete_();
}

}  // namespace binary_storage
}  // namespace esphome
