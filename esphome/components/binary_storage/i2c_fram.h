#pragma once

#include "binary_storage.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace binary_storage {

/**
 * @brief I2C FRAM storage device (Fujitsu MB85RC, Cypress FM24 series)
 *
 * Ferroelectric RAM with unique characteristics:
 * - 10^14 write cycles (100 trillion!)
 * - Instant writes (no page delays)
 * - No erase needed
 * - 10-38 year data retention
 *
 * Supports addressing modes: 9/11/16/32-bit
 */
class I2CFram : public BinaryStorage, public i2c::I2CDevice {
 public:
  I2CFram() = default;

  // Component lifecycle
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  //========================================================================
  // Configuration
  //========================================================================

  /**
   * @brief Set FRAM model/type
   *
   * @param model Model string (e.g., "MB85RC256", "FM24CL64")
   */
  void set_model(const std::string &model) { this->model_ = model; }

  /**
   * @brief Set capacity in bytes (if not auto-detected)
   *
   * @param capacity Capacity in bytes
   */
  void set_capacity(uint32_t capacity) { this->capacity_ = capacity; }

  /**
   * @brief Set addressing mode
   *
   * @param bits Addressing bits (9, 11, 16, or 32)
   */
  void set_addressing_bits(uint8_t bits) { this->addressing_bits_ = bits; }

  //========================================================================
  // BinaryStorage Interface
  //========================================================================

  uint32_t get_capacity() const override { return this->capacity_; }
  const char *get_device_name() const override { return this->model_.c_str(); }
  const char *get_device_type() const override { return "fram"; }
  uint32_t get_page_size() const override { return 1; }  // FRAM has no page limit

  bool read(uint32_t address, uint8_t *data, size_t length) override;
  bool write(uint32_t address, const uint8_t *data, size_t length) override;

  // FRAM-specific features (no delays, no erase needed)
  bool is_ready() override { return true; }
  bool sync() override { return true; }
  bool erase_block(uint32_t address) override { return true; }

  //========================================================================
  // FRAM-Specific Metadata
  //========================================================================

  /**
   * @brief Get manufacturer ID from device
   *
   * @return Manufacturer ID (0x00A = Fujitsu, 0x004 = Cypress)
   */
  uint16_t get_manufacturer_id();

  /**
   * @brief Get product ID from device
   *
   * @return Product ID
   */
  uint16_t get_product_id();

  /**
   * @brief Get density from device (used for auto-detection)
   *
   * @return Density code (size = 2^density KB)
   */
  uint16_t get_density();

  /**
   * @brief Fill entire FRAM with a value
   *
   * Optimized for FRAM's fast writes
   *
   * @param value Byte value to fill (default 0)
   * @return Number of bytes written
   */
  uint32_t clear(uint8_t value = 0);

  /**
   * @brief Enter sleep mode (power saving)
   */
  void sleep();

  /**
   * @brief Wake up from sleep mode
   *
   * @param trec Recovery time in microseconds (default 400us)
   * @return true if device responds after wake
   */
  bool wakeup(uint32_t trec = 400);

 protected:
  //========================================================================
  // Device Configuration
  //========================================================================

  std::string model_{"FRAM"};
  uint32_t capacity_{0};
  uint8_t addressing_bits_{16};  // 9, 11, 16, or 32

  //========================================================================
  // Internal Helpers (from existing FRAM component)
  //========================================================================

  /**
   * @brief Write a block of data to FRAM
   *
   * @param memaddr Memory address
   * @param obj Data buffer
   * @param size Number of bytes
   */
  void write_block_(uint32_t memaddr, uint8_t *obj, uint8_t size);

  /**
   * @brief Read a block of data from FRAM
   *
   * @param memaddr Memory address
   * @param obj Data buffer
   * @param size Number of bytes
   */
  void read_block_(uint32_t memaddr, uint8_t *obj, uint8_t size);

  /**
   * @brief Read metadata from FRAM special address
   *
   * @param field Field to read (0=manufacturer, 1=product, 2=density)
   * @return Metadata value
   */
  uint16_t get_meta_data_(uint8_t field);

  /**
   * @brief Calculate I2C device address for memory address
   *
   * For 9/11-bit addressing, some address bits are in I2C device address
   *
   * @param mem_address Memory address
   * @return I2C device address
   */
  uint8_t get_device_address_for_memory_(uint32_t mem_address) const;
};

}  // namespace binary_storage
}  // namespace esphome
