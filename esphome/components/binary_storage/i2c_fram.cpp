#include "i2c_fram.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <algorithm>
#include <vector>

namespace esphome {
namespace binary_storage {

static const char *const TAG = "i2c_fram";

// FRAM special addresses
static constexpr uint8_t FRAM_SLAVE_ID = 0x7C;   // 0xF8 >> 1
static constexpr uint8_t FRAM_SLEEP_CMD = 0x86;  //

void I2CFram::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2C FRAM...");

  // Try to auto-detect size from metadata if not configured
  if (this->capacity_ == 0) {
    uint16_t density = this->get_density();
    if (density > 0) {
      this->capacity_ = (1UL << density) * 1024UL;  // Convert density to bytes
      ESP_LOGI(TAG, "Auto-detected size: %u KB (%u bytes)", (1UL << density), this->capacity_);
    } else {
      ESP_LOGW(TAG, "Could not auto-detect size, set it in config!");
    }
  }

  // Verify device is present
  i2c::ErrorCode err = this->bus_->writev(this->address_, nullptr, 0);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Device not found at address 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // Call base class setup
  BinaryStorage::setup();

  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  Manufacturer ID: 0x%04X", this->get_manufacturer_id());
  ESP_LOGCONFIG(TAG, "  Product ID: 0x%04X", this->get_product_id());
  ESP_LOGCONFIG(TAG, "  Addressing: %u-bit", this->addressing_bits_);
}

void I2CFram::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C FRAM:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  Capacity: %u bytes (%.1f KB)", this->capacity_, this->capacity_ / 1024.0f);
  ESP_LOGCONFIG(TAG, "  Addressing: %u-bit", this->addressing_bits_);
  ESP_LOGCONFIG(TAG, "  Manufacturer: 0x%04X", this->get_manufacturer_id());
  ESP_LOGCONFIG(TAG, "  Product: 0x%04X", this->get_product_id());

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
  }
}

uint8_t I2CFram::get_device_address_for_memory_(uint32_t mem_address) const {
  uint8_t device_addr = this->address_;

  if (this->addressing_bits_ == 9) {
    // Bit 8 of memory address goes into A0 of device address
    device_addr |= (mem_address >> 8) & 0x01;
  } else if (this->addressing_bits_ == 11) {
    // Bits 8-10 of memory address go into A0-A2 of device address
    device_addr |= (mem_address >> 8) & 0x07;
  } else if (this->addressing_bits_ == 32) {
    // Bit 16 of memory address modifies device address
    if (mem_address & 0x00010000)
      device_addr += 0x01;
  }

  return device_addr;
}

void I2CFram::write_block_(uint32_t memaddr, uint8_t *obj, uint8_t size) {
  uint8_t device_addr = this->get_device_address_for_memory_(memaddr);

  // Build write buffer: [address bytes] + [data]
  std::vector<uint8_t> buffer;
  buffer.reserve(size + 2);

  if (this->addressing_bits_ <= 11) {
    // 9/11-bit addressing: single address byte
    buffer.push_back(memaddr & 0xFF);
  } else {
    // 16/32-bit addressing: two address bytes
    buffer.push_back((memaddr >> 8) & 0xFF);
    buffer.push_back(memaddr & 0xFF);
  }

  // Append data
  buffer.insert(buffer.end(), obj, obj + size);

  // Write to device
  this->bus_->writev(device_addr, buffer.data(), buffer.size());
}

void I2CFram::read_block_(uint32_t memaddr, uint8_t *obj, uint8_t size) {
  uint8_t device_addr = this->get_device_address_for_memory_(memaddr);

  if (this->addressing_bits_ <= 11) {
    // 9/11-bit addressing: single address byte
    uint8_t addr_byte = memaddr & 0xFF;
    this->bus_->write_readv(device_addr, &addr_byte, 1, obj, size);
  } else {
    // 16/32-bit addressing: two address bytes
    uint8_t addr_bytes[2] = {(uint8_t) ((memaddr >> 8) & 0xFF), (uint8_t) (memaddr & 0xFF)};
    this->bus_->write_readv(device_addr, addr_bytes, 2, obj, size);
  }
}

uint16_t I2CFram::get_meta_data_(uint8_t field) {
  if (field > 2)
    return 0;

  uint8_t addr = this->address_ << 1;
  uint8_t data[3] = {0, 0, 0};

  i2c::ErrorCode err = this->bus_->write_readv(FRAM_SLAVE_ID, &addr, 1, data, 3);
  if (err != i2c::ERROR_OK)
    return 0;

  // MANUFACTURER: [....MMMM][MMMMDDDD][PPPPPPPP]
  if (field == 0)
    return (data[0] << 4) + (data[1] >> 4);

  // PRODUCT ID
  if (field == 1)
    return ((data[1] & 0x0F) << 8) + data[2];

  // DENSITY (size = 2^density KB)
  if (field == 2)
    return data[1] & 0x0F;

  return 0;
}

uint16_t I2CFram::get_manufacturer_id() { return this->get_meta_data_(0); }

uint16_t I2CFram::get_product_id() { return this->get_meta_data_(1); }

uint16_t I2CFram::get_density() { return this->get_meta_data_(2); }

bool I2CFram::read(uint32_t address, uint8_t *data, size_t length) {
  if (!this->is_valid_address_(address, length)) {
    ESP_LOGE(TAG, "Read address out of bounds: 0x%X + %u", address, length);
    return false;
  }

  // Read in 24-byte blocks (optimal for I2C)
  const uint8_t blocksize = 24;
  uint8_t *p = data;

  while (length >= blocksize) {
    this->read_block_(address, p, blocksize);
    address += blocksize;
    p += blocksize;
    length -= blocksize;
  }

  // Read remainder
  if (length > 0) {
    this->read_block_(address, p, length);
  }

  return true;
}

bool I2CFram::write(uint32_t address, const uint8_t *data, size_t length) {
  if (!this->is_valid_address_(address, length)) {
    ESP_LOGE(TAG, "Write address out of bounds: 0x%X + %u", address, length);
    return false;
  }

  // FRAM can write any size instantly (no page boundaries!)
  // Write in 24-byte blocks for I2C efficiency
  const uint8_t blocksize = 24;
  const uint8_t *p = data;

  while (length >= blocksize) {
    this->write_block_(address, const_cast<uint8_t *>(p), blocksize);
    address += blocksize;
    p += blocksize;
    length -= blocksize;
  }

  // Write remainder
  if (length > 0) {
    this->write_block_(address, const_cast<uint8_t *>(p), length);
  }

  return true;
}

uint32_t I2CFram::clear(uint8_t value) {
  // Optimized clear for FRAM (can write full speed)
  uint8_t buffer[16];
  std::fill(buffer, buffer + 16, value);

  uint32_t address = 0;
  uint32_t end = this->capacity_;

  while (address < end) {
    size_t chunk_size = std::min((size_t) 16, (size_t) (end - address));
    this->write_block_(address, buffer, chunk_size);
    address += chunk_size;
  }

  return end;
}

void I2CFram::sleep() {
  // FRAM sleep command (page 12 of datasheet)
  // Command: S 0xF8 A address A S 0x86 A P
  uint8_t addr = this->address_ << 1;
  this->bus_->write_readv(FRAM_SLAVE_ID, &addr, 1, nullptr, 0);
  this->bus_->write_readv(FRAM_SLEEP_CMD >> 1, nullptr, 0, nullptr, 0);
}

bool I2CFram::wakeup(uint32_t trec) {
  // Any I2C transaction wakes up FRAM
  i2c::ErrorCode err = this->bus_->writev(this->address_, nullptr, 0);
  bool awake = (err == i2c::ERROR_OK);

  if (trec == 0)
    return awake;

  // Wait recovery time
  delayMicroseconds(trec);

  // Check recovery OK
  err = this->bus_->writev(this->address_, nullptr, 0);
  return err == i2c::ERROR_OK;
}

}  // namespace binary_storage
}  // namespace esphome
