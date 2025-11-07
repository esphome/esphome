#include "spi_flash.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <algorithm>

namespace esphome {
namespace binary_storage {

static const char *const TAG = "spi_flash";

void SPIFlash::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPI Flash...");

  this->spi_setup();

  // Read JEDEC ID for device identification
  this->jedec_id_ = this->read_jedec_id();

  ESP_LOGCONFIG(TAG, "  JEDEC ID: 0x%06X", this->jedec_id_);
  ESP_LOGCONFIG(TAG, "  Manufacturer: 0x%02X", this->get_manufacturer_id());
  ESP_LOGCONFIG(TAG, "  Device ID: 0x%04X", this->get_device_id());

  // Auto-configure from JEDEC ID if capacity not set
  if (this->capacity_ == 0 && this->jedec_id_ != 0 && this->jedec_id_ != 0xFFFFFF) {
    this->auto_configure_from_jedec_id_();
  }

  if (this->capacity_ == 0) {
    ESP_LOGW(TAG, "Could not auto-detect capacity, set it in config!");
  }

  // Verify device is accessible
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Flash device not responding!");
    this->mark_failed();
    return;
  }

  // Call base class setup
  BinaryStorage::setup();

  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
}

void SPIFlash::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI Flash:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  Capacity: %u bytes (%.1f KB)", this->capacity_, this->capacity_ / 1024.0f);
  ESP_LOGCONFIG(TAG, "  Page Size: %u bytes", this->page_size_);
  ESP_LOGCONFIG(TAG, "  Sector Size: %u bytes", this->sector_size_);
  ESP_LOGCONFIG(TAG, "  JEDEC ID: 0x%06X", this->jedec_id_);
  ESP_LOGCONFIG(TAG, "  Manufacturer: 0x%02X", this->get_manufacturer_id());

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
  }
}

void SPIFlash::auto_configure_from_jedec_id_() {
  uint8_t mfg = this->get_manufacturer_id();
  uint16_t dev_id = this->get_device_id();

  // Common capacity detection (bits 15-8 of device ID often indicate capacity)
  uint8_t capacity_code = (dev_id >> 8) & 0xFF;

  // Standard: capacity = 2^capacity_code bytes
  if (capacity_code >= 11 && capacity_code <= 24) {
    this->capacity_ = 1UL << capacity_code;
    ESP_LOGI(TAG, "Auto-detected capacity: %u bytes (%.1f KB)", this->capacity_, this->capacity_ / 1024.0f);
  }

  // Manufacturer-specific adjustments
  switch (mfg) {
    case 0xEF:  // Winbond
      ESP_LOGD(TAG, "Detected Winbond flash");
      break;
    case 0xC2:  // Macronix
      ESP_LOGD(TAG, "Detected Macronix flash");
      break;
    case 0x1F:  // Adesto
      ESP_LOGD(TAG, "Detected Adesto flash");
      break;
    case 0x20:  // Micron/ST
      ESP_LOGD(TAG, "Detected Micron/ST flash");
      break;
    case 0xC8:  // GigaDevice
      ESP_LOGD(TAG, "Detected GigaDevice flash");
      break;
    default:
      ESP_LOGD(TAG, "Unknown manufacturer: 0x%02X", mfg);
      break;
  }
}

uint32_t SPIFlash::read_jedec_id() {
  this->enable();
  this->write_byte(CMD_JEDEC_ID);
  uint8_t mfg = this->read_byte();
  uint8_t id_high = this->read_byte();
  uint8_t id_low = this->read_byte();
  this->disable();

  return ((uint32_t) mfg << 16) | ((uint32_t) id_high << 8) | id_low;
}

uint8_t SPIFlash::read_status_register() {
  this->enable();
  this->write_byte(CMD_READ_STATUS_REG1);
  uint8_t status = this->read_byte();
  this->disable();
  return status;
}

bool SPIFlash::wait_ready(uint32_t timeout_ms) {
  uint32_t start = millis();

  while (millis() - start < timeout_ms) {
    uint8_t status = this->read_status_register();
    if (!(status & STATUS_BUSY)) {
      return true;
    }
    delay(1);
  }

  ESP_LOGW(TAG, "Wait ready timeout after %u ms", timeout_ms);
  return false;
}

bool SPIFlash::is_ready() {
  uint8_t status = this->read_status_register();
  return !(status & STATUS_BUSY);
}

bool SPIFlash::write_enable() {
  this->enable();
  this->write_byte(CMD_WRITE_ENABLE);
  this->disable();

  // Verify write enable latch is set
  uint8_t status = this->read_status_register();
  if (!(status & STATUS_WEL)) {
    ESP_LOGE(TAG, "Write enable failed!");
    return false;
  }

  return true;
}

void SPIFlash::write_disable() {
  this->enable();
  this->write_byte(CMD_WRITE_DISABLE);
  this->disable();
}

bool SPIFlash::read_data_(uint32_t address, uint8_t *data, size_t length) {
  if (!this->wait_ready()) {
    return false;
  }

  this->enable();
  this->write_byte(CMD_READ_DATA);
  this->write_byte((address >> 16) & 0xFF);
  this->write_byte((address >> 8) & 0xFF);
  this->write_byte(address & 0xFF);
  this->read_array(data, length);
  this->disable();

  return true;
}

bool SPIFlash::read(uint32_t address, uint8_t *data, size_t length) {
  if (!this->is_valid_address_(address, length)) {
    ESP_LOGE(TAG, "Read address out of bounds: 0x%X + %u", address, length);
    return false;
  }

  // Read in chunks for better reliability
  const size_t chunk_size = 256;

  while (length > 0) {
    size_t read_len = std::min(length, chunk_size);

    if (!this->read_data_(address, data, read_len)) {
      return false;
    }

    address += read_len;
    data += read_len;
    length -= read_len;
  }

  return true;
}

bool SPIFlash::write_page_(uint32_t address, const uint8_t *data, size_t length) {
  if (length == 0 || length > this->page_size_) {
    ESP_LOGE(TAG, "Invalid page write length: %u (max %u)", length, this->page_size_);
    return false;
  }

  if (!this->wait_ready()) {
    return false;
  }

  if (!this->write_enable()) {
    return false;
  }

  this->enable();
  this->write_byte(CMD_PAGE_PROGRAM);
  this->write_byte((address >> 16) & 0xFF);
  this->write_byte((address >> 8) & 0xFF);
  this->write_byte(address & 0xFF);
  this->write_array(data, length);
  this->disable();

  // Wait for programming to complete
  return this->wait_ready();
}

bool SPIFlash::write(uint32_t address, const uint8_t *data, size_t length) {
  if (!this->is_valid_address_(address, length)) {
    ESP_LOGE(TAG, "Write address out of bounds: 0x%X + %u", address, length);
    return false;
  }

  // Flash requires page-aligned writes
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

bool SPIFlash::erase_(uint32_t address, uint8_t cmd) {
  if (!this->wait_ready()) {
    return false;
  }

  if (!this->write_enable()) {
    return false;
  }

  this->enable();
  this->write_byte(cmd);
  this->write_byte((address >> 16) & 0xFF);
  this->write_byte((address >> 8) & 0xFF);
  this->write_byte(address & 0xFF);
  this->disable();

  // Wait for erase to complete (can take several seconds for large blocks)
  return this->wait_ready(10000);  // 10 second timeout for erase
}

bool SPIFlash::erase_sector(uint32_t address) {
  ESP_LOGD(TAG, "Erasing sector at 0x%06X", address & ~(this->sector_size_ - 1));
  return this->erase_(address, CMD_SECTOR_ERASE_4K);
}

bool SPIFlash::erase_block_32k(uint32_t address) {
  ESP_LOGD(TAG, "Erasing 32KB block at 0x%06X", address & ~0x7FFF);
  return this->erase_(address, CMD_BLOCK_ERASE_32K);
}

bool SPIFlash::erase_block_64k(uint32_t address) {
  ESP_LOGD(TAG, "Erasing 64KB block at 0x%06X", address & ~0xFFFF);
  return this->erase_(address, CMD_BLOCK_ERASE_64K);
}

bool SPIFlash::erase_block(uint32_t address) {
  // Use sector erase by default (smallest erase unit)
  return this->erase_sector(address);
}

bool SPIFlash::erase_chip() {
  ESP_LOGW(TAG, "Erasing entire chip - ALL DATA WILL BE LOST!");

  if (!this->wait_ready()) {
    return false;
  }

  if (!this->write_enable()) {
    return false;
  }

  this->enable();
  this->write_byte(CMD_CHIP_ERASE);
  this->disable();

  // Chip erase can take a very long time (tens of seconds)
  ESP_LOGI(TAG, "Chip erase in progress...");
  bool success = this->wait_ready(60000);  // 60 second timeout

  if (success) {
    ESP_LOGI(TAG, "Chip erase complete");
  } else {
    ESP_LOGE(TAG, "Chip erase timeout!");
  }

  return success;
}

bool SPIFlash::sync() {
  // Just ensure device is ready (any pending operation is complete)
  return this->wait_ready();
}

void SPIFlash::power_down() {
  this->enable();
  this->write_byte(CMD_POWER_DOWN);
  this->disable();

  // Device requires 3us to enter power down mode
  delayMicroseconds(10);

  ESP_LOGD(TAG, "Entered power down mode");
}

void SPIFlash::wake_up() {
  this->enable();
  this->write_byte(CMD_RELEASE_POWER_DOWN);
  this->disable();

  // Device requires 3us to wake up
  delayMicroseconds(10);

  ESP_LOGD(TAG, "Woke up from power down");
}

}  // namespace binary_storage
}  // namespace esphome
