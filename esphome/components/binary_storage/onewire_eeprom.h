#pragma once

#include "binary_storage.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <vector>

namespace esphome {
namespace binary_storage {

/**
 * @brief OneWire EEPROM storage device (DS2431, DS2433, DS28E07)
 *
 * Dallas 1-Wire EEPROM with:
 * - Only 1 GPIO pin needed!
 * - Unique 64-bit ROM ID per device
 * - 1Kbit to 32Kbit capacity
 * - Scratchpad write with verification
 * - ~100,000 write cycles
 * - Lower speed (~16kbps) but GPIO-efficient
 *
 * Supports DS2431 (1KB), DS2433 (4KB), DS28E07 (1KB + EEPROM pages)
 */
class OneWireEEPROM : public BinaryStorage {
 public:
  OneWireEEPROM() = default;

  // Component lifecycle
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  //========================================================================
  // Configuration
  //========================================================================

  /**
   * @brief Set OneWire pin
   *
   * @param pin GPIO pin for OneWire bus
   */
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }

  /**
   * @brief Set EEPROM model/type
   *
   * @param model Model string (e.g., "DS2431", "DS2433")
   */
  void set_model(const std::string &model) { this->model_ = model; }

  /**
   * @brief Set capacity in bytes
   *
   * @param capacity Capacity in bytes
   */
  void set_capacity(uint32_t capacity) { this->capacity_ = capacity; }

  /**
   * @brief Set page size (scratchpad size)
   *
   * @param page_size Page size in bytes (typically 8 or 32)
   */
  void set_page_size(uint32_t page_size) { this->page_size_ = page_size; }

  /**
   * @brief Set 64-bit ROM address (if multiple devices on bus)
   *
   * @param address ROM address
   */
  void set_address(uint64_t address) { this->rom_address_ = address; }

  //========================================================================
  // BinaryStorage Interface
  //========================================================================

  uint32_t get_capacity() const override { return this->capacity_; }
  const char *get_device_name() const override { return this->model_.c_str(); }
  const char *get_device_type() const override { return "onewire_eeprom"; }
  uint32_t get_page_size() const override { return this->page_size_; }

  bool read(uint32_t address, uint8_t *data, size_t length) override;
  bool write(uint32_t address, const uint8_t *data, size_t length) override;
  bool is_ready() override;
  bool sync() override { return true; }  // No buffering
  bool erase_block(uint32_t address) override { return true; }  // No erase needed

  //========================================================================
  // OneWire-Specific Operations
  //========================================================================

  /**
   * @brief Read 64-bit ROM ID from device
   *
   * @return ROM ID (family code + serial + CRC)
   */
  uint64_t read_rom_id();

  /**
   * @brief Get family code from ROM ID
   *
   * @return Family code (0x23 for DS2433, 0x2D for DS2431, etc.)
   */
  uint8_t get_family_code() const { return this->rom_address_ & 0xFF; }

  /**
   * @brief Verify device is present on bus
   *
   * @return true if device responds
   */
  bool verify_device();

 protected:
  //========================================================================
  // Device Configuration
  //========================================================================

  InternalGPIOPin *pin_{nullptr};
  std::string model_{"ONEWIRE_EEPROM"};
  uint32_t capacity_{0};
  uint32_t page_size_{8};  // Scratchpad size (8 or 32 bytes typically)
  uint64_t rom_address_{0};  // 64-bit ROM ID
  bool skip_rom_{true};      // Use Skip ROM if only one device on bus

  //========================================================================
  // OneWire EEPROM Commands (Dallas/Maxim standard)
  //========================================================================

  static constexpr uint8_t CMD_READ_ROM = 0x33;            // Read 64-bit ROM
  static constexpr uint8_t CMD_SKIP_ROM = 0xCC;            // Skip ROM (single device)
  static constexpr uint8_t CMD_MATCH_ROM = 0x55;           // Match ROM (multiple devices)
  static constexpr uint8_t CMD_SEARCH_ROM = 0xF0;          // Search for devices
  static constexpr uint8_t CMD_READ_MEMORY = 0xF0;         // Read memory
  static constexpr uint8_t CMD_WRITE_SCRATCHPAD = 0x0F;    // Write to scratchpad
  static constexpr uint8_t CMD_READ_SCRATCHPAD = 0xAA;     // Read from scratchpad
  static constexpr uint8_t CMD_COPY_SCRATCHPAD = 0x55;     // Copy scratchpad to memory

  // Family codes
  static constexpr uint8_t FAMILY_DS2431 = 0x2D;  // 1KB EEPROM
  static constexpr uint8_t FAMILY_DS2433 = 0x23;  // 4KB EEPROM
  static constexpr uint8_t FAMILY_DS28E07 = 0x1C;  // 1KB + EEPROM pages

  //========================================================================
  // OneWire Protocol Implementation
  //========================================================================

  /**
   * @brief Initialize OneWire bus
   *
   * @return true if device present
   */
  bool onewire_reset();

  /**
   * @brief Write a byte to OneWire bus
   *
   * @param data Byte to write
   */
  void onewire_write_byte(uint8_t data);

  /**
   * @brief Read a byte from OneWire bus
   *
   * @return Byte read
   */
  uint8_t onewire_read_byte();

  /**
   * @brief Write a bit to OneWire bus
   *
   * @param bit Bit to write (0 or 1)
   */
  void onewire_write_bit(bool bit);

  /**
   * @brief Read a bit from OneWire bus
   *
   * @return Bit read (0 or 1)
   */
  bool onewire_read_bit();

  /**
   * @brief Select device on bus (Skip ROM or Match ROM)
   */
  void onewire_select();

  /**
   * @brief Calculate CRC-8 (Dallas/Maxim)
   *
   * @param data Data buffer
   * @param length Data length
   * @return CRC-8 value
   */
  uint8_t crc8(const uint8_t *data, size_t length);

  /**
   * @brief Calculate CRC-16 (for scratchpad verification)
   *
   * @param data Data buffer
   * @param length Data length
   * @return CRC-16 value
   */
  uint16_t crc16(const uint8_t *data, size_t length);

  //========================================================================
  // Memory Access Helpers
  //========================================================================

  /**
   * @brief Write to scratchpad
   *
   * @param address Target address (2 bytes)
   * @param data Data to write
   * @param length Length (max page_size_)
   * @return true on success
   */
  bool write_scratchpad_(uint16_t address, const uint8_t *data, size_t length);

  /**
   * @brief Read scratchpad
   *
   * @param ta1 Target address byte 1
   * @param ta2 Target address byte 2
   * @param es E/S byte
   * @param data Buffer for data
   * @param length Buffer size
   * @return true on success
   */
  bool read_scratchpad_(uint8_t &ta1, uint8_t &ta2, uint8_t &es, uint8_t *data, size_t length);

  /**
   * @brief Copy scratchpad to memory
   *
   * @param ta1 Target address byte 1
   * @param ta2 Target address byte 2
   * @param es E/S byte
   * @return true on success
   */
  bool copy_scratchpad_(uint8_t ta1, uint8_t ta2, uint8_t es);

  /**
   * @brief Read memory directly
   *
   * @param address Starting address
   * @param data Buffer for data
   * @param length Number of bytes
   * @return true on success
   */
  bool read_memory_(uint16_t address, uint8_t *data, size_t length);
};

}  // namespace binary_storage
}  // namespace esphome
