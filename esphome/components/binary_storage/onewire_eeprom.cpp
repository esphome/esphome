#include "esphome/core/defines.h"
#include "onewire_eeprom.h"

#ifdef USE_BINARY_STORAGE_ONEWIRE_EEPROM

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>

namespace esphome::binary_storage {

static const char *const TAG = "onewire_eeprom";

//========================================================================
// Component Lifecycle
//========================================================================

void OneWireEEPROM::setup() {
  ESP_LOGCONFIG(TAG, "Setting up OneWire EEPROM '%s'...", this->model_.c_str());

  // Setup GPIO pin
  if (this->pin_ == nullptr) {
    ESP_LOGE(TAG, "OneWire pin not configured!");
    this->mark_failed();
    return;
  }

  this->pin_->setup();
  this->pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN);

  // Verify device is present
  if (!this->onewire_reset_()) {
    ESP_LOGE(TAG, "No OneWire device found!");
    this->mark_failed();
    return;
  }

  // Read ROM ID if not set
  if (this->rom_address_ == 0) {
    this->rom_address_ = this->read_rom_id();
    if (this->rom_address_ == 0) {
      ESP_LOGW(TAG, "Could not read ROM ID, using Skip ROM mode");
      this->skip_rom_ = true;
    } else {
      ESP_LOGI(TAG, "ROM ID: 0x%016" PRIX64, this->rom_address_);
      this->skip_rom_ = false;
    }
  }

  // Auto-configure from family code if capacity not set
  if (this->capacity_ == 0) {
    uint8_t family = this->get_family_code();
    switch (family) {
      case FAMILY_DS2431:
        this->capacity_ = 1024;  // 1KB (128 bytes x 8 pages)
        this->page_size_ = 8;
        ESP_LOGI(TAG, "Detected DS2431: 1KB, 8-byte pages");
        break;
      case FAMILY_DS2433:
        this->capacity_ = 4096;  // 4KB (512 bytes x 8 pages)
        this->page_size_ = 32;
        ESP_LOGI(TAG, "Detected DS2433: 4KB, 32-byte pages");
        break;
      case FAMILY_DS28E07:
        this->capacity_ = 1024;  // 1KB
        this->page_size_ = 8;
        ESP_LOGI(TAG, "Detected DS28E07: 1KB, 8-byte pages");
        break;
      default:
        ESP_LOGW(TAG, "Unknown family code: 0x%02X, please set capacity manually", family);
        break;
    }
  }

  if (this->capacity_ == 0) {
    ESP_LOGE(TAG, "EEPROM capacity not set!");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "OneWire EEPROM setup complete");
}

void OneWireEEPROM::dump_config() {
  ESP_LOGCONFIG(TAG, "OneWire EEPROM:");
  LOG_PIN("  Data Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  Capacity: %" PRIu32 " bytes", this->capacity_);
  ESP_LOGCONFIG(TAG, "  Page Size: %" PRIu32 " bytes", (uint32_t) this->page_size_);
  ESP_LOGCONFIG(TAG, "  ROM ID: 0x%016" PRIX64, this->rom_address_);
  ESP_LOGCONFIG(TAG, "  Family Code: 0x%02X", this->get_family_code());
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->skip_rom_ ? "Skip ROM (single device)" : "Match ROM (addressed)");

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup failed!");
  }
}

//========================================================================
// BinaryStorage Interface
//========================================================================

storage::StorageError OneWireEEPROM::read(uint64_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred) {
  if (!this->is_valid_address_(offset, len))
    return storage::StorageError::INVALID_ARGS;
  bool ok = this->read_raw(static_cast<uint32_t>(offset), buf, len);
  if (bytes_transferred != nullptr)
    *bytes_transferred = ok ? len : 0;
  return ok ? storage::StorageError::OK : storage::StorageError::READ_ERROR;
}

storage::StorageError OneWireEEPROM::write(uint64_t offset, const uint8_t *buf, size_t len, size_t *bytes_transferred) {
  if (!this->is_valid_address_(offset, len))
    return storage::StorageError::INVALID_ARGS;
  bool ok = this->write_raw(static_cast<uint32_t>(offset), buf, len);
  if (bytes_transferred != nullptr)
    *bytes_transferred = ok ? len : 0;
  return ok ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

storage::StorageError OneWireEEPROM::erase(uint64_t offset, size_t len) { return storage::StorageError::OK; }

storage::StorageError OneWireEEPROM::format() { return this->BinaryStorage::format(); }

bool OneWireEEPROM::read_raw(uint32_t address, uint8_t *data, size_t length) {
  if (address + length > this->capacity_) {
    ESP_LOGE(TAG, "Read overflow: address 0x%04" PRIx32 " + length %" PRIu32 " > capacity %" PRIu32, address,
             (uint32_t) length, this->capacity_);
    return false;
  }

  return this->read_memory_(address, data, length);
}

bool OneWireEEPROM::write_raw(uint32_t address, const uint8_t *data, size_t length) {
  if (address + length > this->capacity_) {
    ESP_LOGE(TAG, "Write overflow: address 0x%04" PRIx32 " + length %" PRIu32 " > capacity %" PRIu32, address,
             (uint32_t) length, this->capacity_);
    return false;
  }

  // Write in page-sized chunks
  while (length > 0) {
    // Calculate bytes to page boundary
    uint32_t page_boundary = ((address / this->page_size_) + 1) * this->page_size_;
    size_t bytes_to_boundary = page_boundary - address;
    size_t write_len = std::min((size_t) bytes_to_boundary, length);
    write_len = std::min(write_len, (size_t) this->page_size_);

    ESP_LOGV(TAG, "Writing %" PRIu32 " bytes at 0x%04" PRIx32, (uint32_t) write_len, address);

    // Write to scratchpad
    if (!this->write_scratchpad_(address, data, write_len)) {
      ESP_LOGE(TAG, "Write scratchpad failed at 0x%04" PRIx32, address);
      return false;
    }

    // Read scratchpad for verification
    uint8_t ta1, ta2, es;
    uint8_t verify_buffer[32];  // Max page size
    if (!this->read_scratchpad_(ta1, ta2, es, verify_buffer, write_len)) {
      ESP_LOGE(TAG, "Read scratchpad failed at 0x%04" PRIx32, address);
      return false;
    }

    // Verify data
    if (memcmp(data, verify_buffer, write_len) != 0) {
      ESP_LOGE(TAG, "Scratchpad data mismatch at 0x%04" PRIx32, address);
      return false;
    }

    // Copy scratchpad to memory
    if (!this->copy_scratchpad_(ta1, ta2, es)) {
      ESP_LOGE(TAG, "Copy scratchpad failed at 0x%04" PRIx32, address);
      return false;
    }

    // Wait for programming (typ. 10ms)
    delay(12);

    address += write_len;
    data += write_len;
    length -= write_len;
  }

  return true;
}

bool OneWireEEPROM::is_ready() { return this->verify_device(); }

//========================================================================
// OneWire-Specific Operations
//========================================================================

uint64_t OneWireEEPROM::read_rom_id() {
  if (!this->onewire_reset_()) {
    return 0;
  }

  // Send Read ROM command
  this->onewire_write_byte_(CMD_READ_ROM);

  // Read 8 bytes (64 bits)
  uint64_t rom = 0;
  for (int i = 0; i < 8; i++) {
    rom |= ((uint64_t) this->onewire_read_byte_()) << (i * 8);
  }

  // Verify CRC
  uint8_t rom_bytes[8];
  for (int i = 0; i < 8; i++) {
    rom_bytes[i] = (rom >> (i * 8)) & 0xFF;
  }
  uint8_t crc = this->crc8_(rom_bytes, 7);
  if (crc != rom_bytes[7]) {
    ESP_LOGE(TAG, "ROM CRC mismatch: calculated 0x%02X, got 0x%02X", crc, rom_bytes[7]);
    return 0;
  }

  return rom;
}

bool OneWireEEPROM::verify_device() { return this->onewire_reset_(); }

//========================================================================
// OneWire Protocol Implementation
//========================================================================

bool OneWireEEPROM::onewire_reset_() {
  // Pull bus low for 480-960μs (reset pulse)
  this->pin_->digital_write(false);
  delayMicroseconds(480);

  // Release bus and wait for presence pulse (60-240μs)
  this->pin_->digital_write(true);
  delayMicroseconds(70);

  // Sample bus (should be low if device present)
  bool present = !this->pin_->digital_read();

  // Wait for presence pulse to finish
  delayMicroseconds(410);

  return present;
}

void OneWireEEPROM::onewire_write_byte_(uint8_t data) {
  for (int i = 0; i < 8; i++) {
    this->onewire_write_bit_(data & 0x01);
    data >>= 1;
  }
}

uint8_t OneWireEEPROM::onewire_read_byte_() {
  uint8_t data = 0;
  for (int i = 0; i < 8; i++) {
    data >>= 1;
    if (this->onewire_read_bit_()) {
      data |= 0x80;
    }
  }
  return data;
}

void OneWireEEPROM::onewire_write_bit_(bool bit) {
  if (bit) {
    // Write 1: 1-15μs low, then release
    this->pin_->digital_write(false);
    delayMicroseconds(10);
    this->pin_->digital_write(true);
    delayMicroseconds(55);
  } else {
    // Write 0: 60-120μs low
    this->pin_->digital_write(false);
    delayMicroseconds(65);
    this->pin_->digital_write(true);
    delayMicroseconds(5);
  }
}

bool OneWireEEPROM::onewire_read_bit_() {
  // Pull bus low for 1-15μs
  this->pin_->digital_write(false);
  delayMicroseconds(3);

  // Release bus
  this->pin_->digital_write(true);

  // Sample within 15μs
  delayMicroseconds(10);
  bool bit = this->pin_->digital_read();

  // Recovery time
  delayMicroseconds(53);

  return bit;
}

void OneWireEEPROM::onewire_select_() {
  if (this->skip_rom_) {
    // Skip ROM command (single device on bus)
    this->onewire_write_byte_(CMD_SKIP_ROM);
  } else {
    // Match ROM command (specific device)
    this->onewire_write_byte_(CMD_MATCH_ROM);
    // Send ROM address
    for (int i = 0; i < 8; i++) {
      this->onewire_write_byte_((this->rom_address_ >> (i * 8)) & 0xFF);
    }
  }
}

uint8_t OneWireEEPROM::crc8_(const uint8_t *data, size_t length) {
  uint8_t crc = 0;

  for (size_t i = 0; i < length; i++) {
    uint8_t byte = data[i];
    crc ^= byte;

    for (int j = 0; j < 8; j++) {
      if (crc & 0x01) {
        crc = (crc >> 1) ^ 0x8C;  // Dallas/Maxim CRC-8 polynomial
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

uint16_t OneWireEEPROM::crc16_(const uint8_t *data, size_t length) {
  uint16_t crc = 0;

  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];

    for (int j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;  // CRC-16-IBM/ANSI polynomial
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

//========================================================================
// Memory Access Helpers
//========================================================================

bool OneWireEEPROM::write_scratchpad_(uint16_t address, const uint8_t *data, size_t length) {
  if (length > this->page_size_) {
    return false;
  }

  if (!this->onewire_reset_()) {
    return false;
  }

  this->onewire_select_();
  this->onewire_write_byte_(CMD_WRITE_SCRATCHPAD);

  // Send target address (TA1, TA2)
  this->onewire_write_byte_(address & 0xFF);
  this->onewire_write_byte_((address >> 8) & 0xFF);

  // Send data
  for (size_t i = 0; i < length; i++) {
    this->onewire_write_byte_(data[i]);
  }

  return true;
}

bool OneWireEEPROM::read_scratchpad_(uint8_t &ta1, uint8_t &ta2, uint8_t &es, uint8_t *data, size_t length) {
  if (!this->onewire_reset_()) {
    return false;
  }

  this->onewire_select_();
  this->onewire_write_byte_(CMD_READ_SCRATCHPAD);

  // Read TA1, TA2, E/S
  ta1 = this->onewire_read_byte_();
  ta2 = this->onewire_read_byte_();
  es = this->onewire_read_byte_();

  // Read data
  for (size_t i = 0; i < length; i++) {
    data[i] = this->onewire_read_byte_();
  }

  // Read CRC16
  uint8_t crc_low = this->onewire_read_byte_();
  uint8_t crc_high = this->onewire_read_byte_();
  uint16_t crc_read = (crc_high << 8) | crc_low;

  // Verify CRC
  uint8_t verify_buffer[3 + 32];  // TA1, TA2, E/S + data
  verify_buffer[0] = ta1;
  verify_buffer[1] = ta2;
  verify_buffer[2] = es;
  memcpy(&verify_buffer[3], data, length);

  uint16_t crc_calc = this->crc16_(verify_buffer, 3 + length);
  if (crc_calc != crc_read) {
    ESP_LOGE(TAG, "Scratchpad CRC mismatch: calculated 0x%04X, got 0x%04X", crc_calc, crc_read);
    return false;
  }

  return true;
}

bool OneWireEEPROM::copy_scratchpad_(uint8_t ta1, uint8_t ta2, uint8_t es) {
  if (!this->onewire_reset_()) {
    return false;
  }

  this->onewire_select_();
  this->onewire_write_byte_(CMD_COPY_SCRATCHPAD);

  // Send authorization code (TA1, TA2, E/S)
  this->onewire_write_byte_(ta1);
  this->onewire_write_byte_(ta2);
  this->onewire_write_byte_(es);

  return true;
}

bool OneWireEEPROM::read_memory_(uint16_t address, uint8_t *data, size_t length) {
  if (!this->onewire_reset_()) {
    return false;
  }

  this->onewire_select_();
  this->onewire_write_byte_(CMD_READ_MEMORY);

  // Send address (TA1, TA2)
  this->onewire_write_byte_(address & 0xFF);
  this->onewire_write_byte_((address >> 8) & 0xFF);

  // Read data
  for (size_t i = 0; i < length; i++) {
    data[i] = this->onewire_read_byte_();
  }

  return true;
}

}  // namespace esphome::binary_storage

#endif  // USE_BINARY_STORAGE_ONEWIRE_EEPROM
