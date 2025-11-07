#include "spi_mram.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace binary_storage {

static const char *const TAG = "spi_mram";

//========================================================================
// Component Lifecycle
//========================================================================

void SPIMRAM::setup() {
  this->spi_setup();

  ESP_LOGCONFIG(TAG, "Setting up SPI MRAM '%s'...", this->model_.c_str());

  // Check if device responds
  uint8_t status = this->read_status_register();
  ESP_LOGV(TAG, "Status register: 0x%02X", status);

  // Verify capacity is set
  if (this->capacity_ == 0) {
    ESP_LOGE(TAG, "MRAM capacity not set! Please specify capacity in configuration.");
    this->mark_failed();
    return;
  }

  // Check addressing mode is valid
  if (this->addressing_bits_ != 16 && this->addressing_bits_ != 24) {
    ESP_LOGE(TAG, "Invalid addressing bits: %u (must be 16 or 24)", this->addressing_bits_);
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "SPI MRAM setup complete");
}

void SPIMRAM::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI MRAM:");
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  Capacity: %u bytes (%" PRIu32 " KB)", this->capacity_, this->capacity_ / 1024);
  ESP_LOGCONFIG(TAG, "  Addressing: %u-bit", this->addressing_bits_);
  ESP_LOGCONFIG(TAG, "  SPI Speed: %u Hz", this->get_data_rate());
  ESP_LOGCONFIG(TAG, "  Features: Unlimited write cycles, instant writes, no erase needed");

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

bool SPIMRAM::read(uint32_t address, uint8_t *data, size_t length) {
  if (address + length > this->capacity_) {
    ESP_LOGE(TAG, "Read overflow: address 0x%04" PRIX32 " + length %u > capacity %" PRIu32, address, length,
             this->capacity_);
    return false;
  }

  return this->read_data_(address, data, length);
}

bool SPIMRAM::write(uint32_t address, const uint8_t *data, size_t length) {
  if (address + length > this->capacity_) {
    ESP_LOGE(TAG, "Write overflow: address 0x%04" PRIX32 " + length %u > capacity %" PRIu32, address, length,
             this->capacity_);
    return false;
  }

  return this->write_data_(address, data, length);
}

//========================================================================
// MRAM-Specific Operations
//========================================================================

uint8_t SPIMRAM::read_status_register() {
  this->enable();
  this->write_byte(CMD_RDSR);
  uint8_t status = this->read_byte();
  this->disable();
  return status;
}

void SPIMRAM::write_status_register(uint8_t value) {
  this->write_enable();
  this->enable();
  this->write_byte(CMD_WRSR);
  this->write_byte(value);
  this->disable();
  this->write_disable();
}

uint32_t SPIMRAM::clear(uint8_t value) {
  ESP_LOGI(TAG, "Clearing MRAM with value 0x%02X...", value);

  // MRAM can write quickly, so we can do larger chunks
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

  ESP_LOGI(TAG, "MRAM cleared successfully");
  return this->capacity_;
}

//========================================================================
// Internal Helpers
//========================================================================

void SPIMRAM::write_enable() {
  this->enable();
  this->write_byte(CMD_WREN);
  this->disable();
}

void SPIMRAM::write_disable() {
  this->enable();
  this->write_byte(CMD_WRDI);
  this->disable();
}

bool SPIMRAM::write_data_(uint32_t address, const uint8_t *data, size_t length) {
  // MRAM can write any size instantly (no page boundaries!)
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

    // MRAM writes are instant - no need to wait!
    // Optionally disable writes (not strictly necessary)
    this->write_disable();

    address += chunk_size;
    data += chunk_size;
    length -= chunk_size;
  }

  return true;
}

bool SPIMRAM::read_data_(uint32_t address, uint8_t *data, size_t length) {
  // MRAM can read any size
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
