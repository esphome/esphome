#include "spi_fram.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace binary_storage {

static const char *const TAG = "spi_fram";

//========================================================================
// Component Lifecycle
//========================================================================

void SPIFram::setup() {
  this->spi_setup();

  ESP_LOGCONFIG(TAG, "Setting up SPI FRAM '%s'...", this->model_.c_str());

  // Check if device responds
  uint8_t status = this->read_status_register();
  ESP_LOGV(TAG, "Status register: 0x%02X", status);

  // Try to read device ID (some FRAMs support this)
  uint32_t device_id = this->read_device_id();
  if (device_id != 0 && device_id != 0xFFFFFF) {
    ESP_LOGI(TAG, "Device ID: 0x%06" PRIX32, device_id);
  }

  // Verify capacity if set
  if (this->capacity_ == 0) {
    ESP_LOGE(TAG, "FRAM capacity not set! Please specify capacity in configuration.");
    this->mark_failed();
    return;
  }

  // Check addressing mode is valid
  if (this->addressing_bits_ != 16 && this->addressing_bits_ != 24) {
    ESP_LOGE(TAG, "Invalid addressing bits: %u (must be 16 or 24)", this->addressing_bits_);
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "SPI FRAM setup complete");
}

void SPIFram::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI FRAM:");
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  Capacity: %u bytes (%" PRIu32 " KB)", this->capacity_, this->capacity_ / 1024);
  ESP_LOGCONFIG(TAG, "  Addressing: %u-bit", this->addressing_bits_);
  ESP_LOGCONFIG(TAG, "  SPI Speed: %u Hz", this->get_data_rate());

  uint8_t status = this->read_status_register();
  ESP_LOGCONFIG(TAG, "  Status Register: 0x%02X", status);
  ESP_LOGCONFIG(TAG, "    Write Enable: %s", (status & STATUS_WEL) ? "Yes" : "No");
  ESP_LOGCONFIG(TAG, "    Block Protect: %u", (status & (STATUS_BP0 | STATUS_BP1)) >> 2);
  ESP_LOGCONFIG(TAG, "    Write Protect: %s", (status & STATUS_WPEN) ? "Enabled" : "Disabled");

  LOG_SPI_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup failed!");
  }
}

//========================================================================
// BinaryStorage Interface
//========================================================================

bool SPIFram::read(uint32_t address, uint8_t *data, size_t length) {
  if (address + length > this->capacity_) {
    ESP_LOGE(TAG, "Read overflow: address 0x%04" PRIX32 " + length %u > capacity %" PRIu32, address, length,
             this->capacity_);
    return false;
  }

  return this->read_data_(address, data, length);
}

bool SPIFram::write(uint32_t address, const uint8_t *data, size_t length) {
  if (address + length > this->capacity_) {
    ESP_LOGE(TAG, "Write overflow: address 0x%04" PRIX32 " + length %u > capacity %" PRIu32, address, length,
             this->capacity_);
    return false;
  }

  return this->write_data_(address, data, length);
}

//========================================================================
// FRAM-Specific Operations
//========================================================================

uint8_t SPIFram::read_status_register() {
  this->enable();
  this->write_byte(CMD_RDSR);
  uint8_t status = this->read_byte();
  this->disable();
  return status;
}

void SPIFram::write_status_register(uint8_t value) {
  this->write_enable();
  this->enable();
  this->write_byte(CMD_WRSR);
  this->write_byte(value);
  this->disable();
  this->write_disable();
}

uint32_t SPIFram::read_device_id() {
  this->enable();
  this->write_byte(CMD_RDID);

  // Read 3 bytes (manufacturer ID + product ID)
  uint8_t mfg = this->read_byte();
  uint8_t prod_h = this->read_byte();
  uint8_t prod_l = this->read_byte();

  this->disable();

  // Return as 24-bit value
  return ((uint32_t) mfg << 16) | ((uint32_t) prod_h << 8) | prod_l;
}

uint32_t SPIFram::clear(uint8_t value) {
  ESP_LOGI(TAG, "Clearing FRAM with value 0x%02X...", value);

  // FRAM can write quickly, so we can do larger chunks
  constexpr size_t CHUNK_SIZE = 256;
  uint8_t buffer[CHUNK_SIZE];
  memset(buffer, value, CHUNK_SIZE);

  uint32_t address = 0;
  while (address < this->capacity_) {
    size_t write_len = std::min(CHUNK_SIZE, (size_t) (this->capacity_ - address));
    if (!this->write(address, buffer, write_len)) {
      ESP_LOGE(TAG, "Clear failed at address 0x%04" PRIX32, address);
      return address;
    }
    address += write_len;
  }

  ESP_LOGI(TAG, "FRAM cleared successfully");
  return this->capacity_;
}

//========================================================================
// Internal Helpers
//========================================================================

void SPIFram::write_enable() {
  this->enable();
  this->write_byte(CMD_WREN);
  this->disable();
}

void SPIFram::write_disable() {
  this->enable();
  this->write_byte(CMD_WRDI);
  this->disable();
}

bool SPIFram::write_data_(uint32_t address, const uint8_t *data, size_t length) {
  // FRAM can write any size instantly (no page boundaries!)
  // But we break it into chunks for SPI efficiency

  constexpr size_t MAX_CHUNK = 256;  // Reasonable SPI transaction size

  while (length > 0) {
    size_t chunk_size = std::min(length, MAX_CHUNK);

    // Enable writes
    this->write_enable();

    // Start write command
    this->enable();
    this->write_byte(CMD_WRITE);

    // Write address (16-bit or 24-bit)
    if (this->addressing_bits_ == 24) {
      this->write_byte((address >> 16) & 0xFF);
    }
    this->write_byte((address >> 8) & 0xFF);
    this->write_byte(address & 0xFF);

    // Write data
    this->write_array(data, chunk_size);
    this->disable();

    // FRAM writes are instant - no need to wait!
    // Optionally disable writes (not strictly necessary)
    this->write_disable();

    address += chunk_size;
    data += chunk_size;
    length -= chunk_size;
  }

  return true;
}

bool SPIFram::read_data_(uint32_t address, uint8_t *data, size_t length) {
  // FRAM can read any size
  constexpr size_t MAX_CHUNK = 256;  // Reasonable SPI transaction size

  while (length > 0) {
    size_t chunk_size = std::min(length, MAX_CHUNK);

    // Start read command
    this->enable();
    this->write_byte(CMD_READ);

    // Write address (16-bit or 24-bit)
    if (this->addressing_bits_ == 24) {
      this->write_byte((address >> 16) & 0xFF);
    }
    this->write_byte((address >> 8) & 0xFF);
    this->write_byte(address & 0xFF);

    // Read data
    this->read_array(data, chunk_size);
    this->disable();

    address += chunk_size;
    data += chunk_size;
    length -= chunk_size;
  }

  return true;
}

}  // namespace binary_storage
}  // namespace esphome
