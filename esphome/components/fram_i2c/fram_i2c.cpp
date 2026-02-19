#include "fram_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nvm {

static const char *const TAG = "nvm.fram_i2c";

void FramI2cPlatform::setup() {
  // Check if FRAM is connected
  if (!this->is_connected()) {
    ESP_LOGE(TAG, "FRAM not found at address 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "FRAM I2C initialized:");
  ESP_LOGCONFIG(TAG, "  Model: %s", model_.name);
  ESP_LOGCONFIG(TAG, "  Size: %u bytes", model_.size_bytes);
  ESP_LOGCONFIG(TAG, "  Address width: %u bits", model_.address_width * 8);

  // Call parent setup to initialize partitions
  NvmPlatform::setup();
}

void FramI2cPlatform::dump_config() {
  ESP_LOGCONFIG(TAG, "FRAM I2C Platform:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  Model: %s", model_.name);
  ESP_LOGCONFIG(TAG, "  Size: %u bytes", model_.size_bytes);
  ESP_LOGCONFIG(TAG, "  Address width: %u bits", model_.address_width * 8);

  // Call parent dump_config to show partitions
  NvmPlatform::dump_config();
}

void FramI2cPlatform::set_model(uint32_t size_bytes) {
  for (const auto &model : FRAM_I2C_MODELS) {
    if (model.size_bytes == size_bytes) {
      model_ = model;
      return;
    }
  }
  ESP_LOGW(TAG, "Unknown FRAM size %u, using default settings", size_bytes);
  model_.size_bytes = size_bytes;
  model_.address_width = 2;  // Default to 16-bit addressing
  model_.name = "Unknown";
}

bool FramI2cPlatform::is_connected() {
  // Try to read one byte to check if device is present
  uint8_t data;
  return this->read_bytes(0, &data, 1);
}

bool FramI2cPlatform::read_bytes(uint32_t memaddr, uint8_t *data, size_t len) {
  if (memaddr + len > model_.size_bytes) {
    ESP_LOGE(TAG, "Read out of bounds: addr=%u, len=%zu, size=%u", memaddr, len, model_.size_bytes);
    return false;
  }

  if (model_.address_width == 2) {
    return this->read_bytes_16(memaddr, data, len);
  } else {
    return this->read_bytes_ext(memaddr, data, len);
  }
}

bool FramI2cPlatform::write_bytes(uint32_t memaddr, const uint8_t *data, size_t len) {
  if (memaddr + len > model_.size_bytes) {
    ESP_LOGE(TAG, "Write out of bounds: addr=%u, len=%zu, size=%u", memaddr, len, model_.size_bytes);
    return false;
  }

  if (model_.address_width == 2) {
    return this->write_bytes_16(memaddr, data, len);
  } else {
    return this->write_bytes_ext(memaddr, data, len);
  }
}

bool FramI2cPlatform::read_bytes_16(uint32_t memaddr, uint8_t *data, size_t len) {
  // Standard I2C FRAM read: [device_addr+W][addr_high][addr_low] then [device_addr+R][data...]
  uint8_t addr_buf[2];
  addr_buf[0] = (memaddr >> 8) & 0xFF;
  addr_buf[1] = memaddr & 0xFF;

  ESP_LOGV(TAG, "Read addr=0x%04X, len=%zu", memaddr, len);
  // Write address, then read data
  i2c::ErrorCode err = this->write_read(addr_buf, 2, data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Read failed at address 0x%04X: error %d", memaddr, err);
    return false;
  }

  return true;
}

bool FramI2cPlatform::write_bytes_16(uint32_t memaddr, const uint8_t *data, size_t len) {
  // Standard I2C FRAM write: [device_addr+W][addr_high][addr_low][data...]
  // FRAM doesn't need page write delays like EEPROM

  ESP_LOGV(TAG, "Write addr=0x%04X, len=%zu", memaddr, len);
  // Prepare buffer: address + data (using unique_ptr to avoid STL vector overhead)
  auto buf = std::make_unique<uint8_t[]>(2 + len);
  buf[0] = (memaddr >> 8) & 0xFF;
  buf[1] = memaddr & 0xFF;
  std::copy(data, data + len, buf.get() + 2);

  i2c::ErrorCode err = this->write(buf.get(), 2 + len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Write failed at address 0x%04X: error %d", memaddr, err);
    return false;
  }

  return true;
}

bool FramI2cPlatform::read_bytes_ext(uint32_t memaddr, uint8_t *data, size_t len) {
  // Extended address FRAM (17/18-bit): address high bits go into device address
  // For 17-bit: device address bits [1:0] become address bits [16:15]
  // For 18-bit: device address bits [1:0] become address bits [17:16]

  uint8_t addr_high = (memaddr >> 16) & 0x03;
  uint8_t modified_address = (this->address_ & 0xFC) | addr_high;

  uint8_t addr_buf[2];
  addr_buf[0] = (memaddr >> 8) & 0xFF;
  addr_buf[1] = memaddr & 0xFF;

  ESP_LOGV(TAG, "Read ext addr=0x%06X, len=%zu, i2c_addr=0x%02X", memaddr, len, modified_address);
  // Use modified address for this transaction
  i2c::ErrorCode err = this->bus_->write_readv(modified_address, addr_buf, 2, data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Extended read failed at address 0x%06X: error %d", memaddr, err);
    return false;
  }

  return true;
}

bool FramI2cPlatform::write_bytes_ext(uint32_t memaddr, const uint8_t *data, size_t len) {
  // Extended address FRAM write
  uint8_t addr_high = (memaddr >> 16) & 0x03;
  uint8_t modified_address = (this->address_ & 0xFC) | addr_high;

  ESP_LOGV(TAG, "Write ext addr=0x%06X, len=%zu, i2c_addr=0x%02X", memaddr, len, modified_address);
  // Using unique_ptr to avoid STL vector overhead
  auto buf = std::make_unique<uint8_t[]>(2 + len);
  buf[0] = (memaddr >> 8) & 0xFF;
  buf[1] = memaddr & 0xFF;
  std::copy(data, data + len, buf.get() + 2);

  i2c::ErrorCode err = this->bus_->write(modified_address, buf.get(), 2 + len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Extended write failed at address 0x%06X: error %d", memaddr, err);
    return false;
  }

  return true;
}

}  // namespace nvm
}  // namespace esphome
