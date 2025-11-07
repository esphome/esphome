#pragma once

#include "binary_storage.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace binary_storage {

/**
 * @brief SPI FRAM storage device (FM25, CY15B series)
 *
 * SPI Ferroelectric RAM with:
 * - Instant writes (no delays)
 * - 10^14 write cycles (100 trillion!)
 * - No erase needed
 * - Standard SPI EEPROM command set
 * - Up to 40MHz SPI clock
 *
 * Supports Cypress/Infineon FM25, CY15B series
 */
class SPIFram : public BinaryStorage, public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_8MHZ> {
 public:
  SPIFram() = default;

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
   * @param model Model string (e.g., "FM25V10", "CY15B104")
   */
  void set_model(const std::string &model) { this->model_ = model; }

  /**
   * @brief Set capacity in bytes
   *
   * @param capacity Capacity in bytes
   */
  void set_capacity(uint32_t capacity) { this->capacity_ = capacity; }

  /**
   * @brief Set addressing mode (16-bit or 24-bit)
   *
   * @param bits Address bits (16 or 24)
   */
  void set_addressing_bits(uint8_t bits) { this->addressing_bits_ = bits; }

  //========================================================================
  // BinaryStorage Interface
  //========================================================================

  uint32_t get_capacity() const override { return this->capacity_; }
  const char *get_device_name() const override { return this->model_.c_str(); }
  const char *get_device_type() const override { return "spi_fram"; }
  uint32_t get_page_size() const override { return 1; }  // No page limit

  bool read(uint32_t address, uint8_t *data, size_t length) override;
  bool write(uint32_t address, const uint8_t *data, size_t length) override;

  // FRAM-specific features (no delays, no erase needed)
  bool is_ready() override { return true; }
  bool sync() override { return true; }
  bool erase_block(uint32_t address) override { return true; }

  //========================================================================
  // FRAM-Specific Operations
  //========================================================================

  /**
   * @brief Read status register
   *
   * @return Status register value
   */
  uint8_t read_status_register();

  /**
   * @brief Write status register
   *
   * @param value New status register value
   */
  void write_status_register(uint8_t value);

  /**
   * @brief Read device ID (if supported)
   *
   * @return Device ID or 0 if not supported
   */
  uint32_t read_device_id();

  /**
   * @brief Fast clear (fill with value)
   *
   * Optimized for FRAM's fast writes
   *
   * @param value Byte value to fill (default 0)
   * @return Number of bytes written
   */
  uint32_t clear(uint8_t value = 0);

 protected:
  //========================================================================
  // Device Configuration
  //========================================================================

  std::string model_{"SPI_FRAM"};
  uint32_t capacity_{0};
  uint8_t addressing_bits_{16};  // 16 or 24

  //========================================================================
  // SPI FRAM Commands (standard SPI EEPROM compatible)
  //========================================================================

  static constexpr uint8_t CMD_WREN = 0x06;   // Set write enable latch
  static constexpr uint8_t CMD_WRDI = 0x04;   // Reset write enable latch
  static constexpr uint8_t CMD_RDSR = 0x05;   // Read status register
  static constexpr uint8_t CMD_WRSR = 0x01;   // Write status register
  static constexpr uint8_t CMD_READ = 0x03;   // Read memory data
  static constexpr uint8_t CMD_WRITE = 0x02;  // Write memory data
  static constexpr uint8_t CMD_RDID = 0x9F;   // Read device ID (if supported)

  // Status register bits
  static constexpr uint8_t STATUS_WEL = 0x02;   // Write enable latch
  static constexpr uint8_t STATUS_BP0 = 0x04;   // Block protect bit 0
  static constexpr uint8_t STATUS_BP1 = 0x08;   // Block protect bit 1
  static constexpr uint8_t STATUS_WPEN = 0x80;  // Write protect enable

  //========================================================================
  // Internal Helpers
  //========================================================================

  /**
   * @brief Enable writes (must be called before write operation)
   */
  void write_enable();

  /**
   * @brief Disable writes
   */
  void write_disable();

  /**
   * @brief Write data to FRAM
   *
   * @param address Starting address
   * @param data Data to write
   * @param length Number of bytes
   * @return true on success
   */
  bool write_data_(uint32_t address, const uint8_t *data, size_t length);

  /**
   * @brief Read data from FRAM
   *
   * @param address Starting address
   * @param data Buffer for read data
   * @param length Number of bytes
   * @return true on success
   */
  bool read_data_(uint32_t address, uint8_t *data, size_t length);
};

}  // namespace binary_storage
}  // namespace esphome
