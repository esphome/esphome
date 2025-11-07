#pragma once

#include "binary_storage.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace binary_storage {

/**
 * @brief I2C EEPROM storage device (AT24C/24LC/24AA series)
 *
 * Supports Microchip/Atmel I2C EEPROM chips:
 * - AT24C01-AT24C512 (Atmel/Microchip)
 * - 24LC01-24LC512 (Microchip standard voltage)
 * - 24AA01-24AA512 (Microchip low voltage)
 * - Compatible with M24C, CAT24C, BR24G, FT24C series
 *
 * Key characteristics:
 * - Page writes with boundaries (8-128 bytes depending on model)
 * - 5ms write delay required after page write
 * - 10^6 write cycles endurance
 * - Different addressing modes: 9/11/16-bit
 */
class I2CEeprom : public BinaryStorage, public i2c::I2CDevice {
 public:
  I2CEeprom() = default;

  // Component lifecycle
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  //========================================================================
  // Configuration
  //========================================================================

  /**
   * @brief Set EEPROM model/type
   *
   * @param model Model string (e.g., "AT24C256", "24LC64")
   */
  void set_model(const std::string &model) { this->model_ = model; }

  /**
   * @brief Set capacity in bytes (if not auto-detected from model)
   *
   * @param capacity Capacity in bytes
   */
  void set_capacity(uint32_t capacity) { this->capacity_ = capacity; }

  /**
   * @brief Set page size for writes (if not auto-detected)
   *
   * @param page_size Page size in bytes (8, 16, 32, 64, 128)
   */
  void set_page_size(uint32_t page_size) { this->page_size_ = page_size; }

  /**
   * @brief Set addressing mode
   *
   * @param mode Addressing mode (9, 11, or 16 bits)
   */
  void set_addressing_bits(uint8_t bits) { this->addressing_bits_ = bits; }

  //========================================================================
  // BinaryStorage Interface
  //========================================================================

  uint32_t get_capacity() const override { return this->capacity_; }
  const char *get_device_name() const override { return this->model_.c_str(); }
  const char *get_device_type() const override { return "eeprom"; }
  uint32_t get_page_size() const override { return this->page_size_; }

  bool is_ready() override;
  bool read(uint32_t address, uint8_t *data, size_t length) override;
  bool write(uint32_t address, const uint8_t *data, size_t length) override;
  bool sync() override;

 protected:
  //========================================================================
  // Device Configuration
  //========================================================================

  std::string model_{"EEPROM"};
  uint32_t capacity_{0};
  uint32_t page_size_{32};     // Default 32 bytes
  uint8_t addressing_bits_{16};  // 9, 11, or 16

  //========================================================================
  // Internal Helpers
  //========================================================================

  /**
   * @brief Auto-configure based on model string
   *
   * Detects capacity, page size, and addressing mode from model name
   */
  void auto_configure_from_model_();

  /**
   * @brief Write a single page (respects page boundaries)
   *
   * @param address Starting address
   * @param data Data to write
   * @param length Number of bytes (must fit in one page)
   * @return true on success
   */
  bool write_page_(uint32_t address, const uint8_t *data, size_t length);

  /**
   * @brief Read block from EEPROM
   *
   * @param address Starting address
   * @param data Buffer for read data
   * @param length Number of bytes to read
   * @return true on success
   */
  bool read_block_(uint32_t address, uint8_t *data, size_t length);

  /**
   * @brief Calculate I2C device address for given memory address
   *
   * For 9/11-bit addressing chips, some address bits are in the I2C device address
   *
   * @param mem_address Memory address
   * @return I2C device address to use
   */
  uint8_t get_device_address_for_memory_(uint32_t mem_address) const;

  /**
   * @brief Poll device until write is complete
   *
   * Uses ACK polling - device will NACK while busy
   *
   * @param timeout_ms Maximum time to wait (default 10ms)
   * @return true if ready, false if timeout
   */
  bool wait_for_write_complete_(uint32_t timeout_ms = 10);

  /**
   * @brief Get number of bytes remaining in current page
   *
   * @param address Current address
   * @return Bytes until page boundary
   */
  uint32_t get_bytes_to_page_boundary_(uint32_t address) const {
    return this->page_size_ - (address % this->page_size_);
  }
};

}  // namespace binary_storage
}  // namespace esphome
