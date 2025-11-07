#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <string>
#include <cstdint>
#include <cstddef>

namespace esphome {
namespace binary_storage {

static const char *const TAG = "binary_storage";

/**
 * @brief Block device operations for LittleFS integration
 *
 * LittleFS requires block-oriented operations. This interface provides
 * the translation layer between byte-addressable storage (FRAM/EEPROM/Flash)
 * and block-oriented filesystem operations.
 */
struct BlockDeviceConfig {
  uint32_t block_size;     ///< Size of each block (typically 256-4096 bytes)
  uint32_t block_count;    ///< Total number of blocks
  uint32_t read_size;      ///< Minimum read size (typically 1)
  uint32_t prog_size;      ///< Minimum program/write size (typically 1 for FRAM, page size for EEPROM/Flash)
  uint32_t lookahead_size; ///< Size for LittleFS lookahead buffer (typically block_count/8)
};

/**
 * @brief Abstract base class for binary storage devices
 *
 * Provides a unified interface for I2C/SPI FRAM, EEPROM, and Flash devices.
 * Handles device-specific quirks (addressing modes, page writes, erase operations)
 * while presenting a simple read/write API to consumers.
 */
class BinaryStorage : public Component {
 public:
  BinaryStorage() = default;

  // Component lifecycle
  void setup() override;
  void loop() override {}
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  //========================================================================
  // Storage Host Integration (soft dependency)
  //========================================================================

  /**
   * @brief Register this device with storage_host as a device node
   *
   * Creates a virtual /dev entry for raw access to this binary_storage device.
   * Only works if storage_host component is present (soft dependency).
   *
   * @param device_node_path Path for device node (e.g., "/dev/fram0")
   */
  void register_with_storage_host(const std::string &device_node_path);

  //========================================================================
  // Device Information
  //========================================================================

  /**
   * @brief Get total storage capacity in bytes
   * @return Total capacity in bytes
   */
  virtual uint32_t get_capacity() const = 0;

  /**
   * @brief Get device name/model
   * @return Device name string (e.g., "FRAM MB85RC256", "EEPROM AT24C256")
   */
  virtual const char *get_device_name() const = 0;

  /**
   * @brief Get device type identifier
   * @return Device type (fram, eeprom, flash, etc.)
   */
  virtual const char *get_device_type() const = 0;

  /**
   * @brief Get page/write size for optimal performance
   * @return Page size in bytes (1 for FRAM, 8-128 for EEPROM, 256 for Flash)
   */
  virtual uint32_t get_page_size() const { return 1; }

  /**
   * @brief Get block/sector erase size (for Flash devices)
   * @return Erase block size in bytes (0 if no erase needed)
   */
  virtual uint32_t get_erase_size() const { return 0; }

  /**
   * @brief Check if device is ready for operations
   * @return true if device is ready, false if busy
   */
  virtual bool is_ready() { return true; }

  //========================================================================
  // Basic I/O Operations
  //========================================================================

  /**
   * @brief Read data from storage
   *
   * @param address Starting address to read from
   * @param data Buffer to store read data
   * @param length Number of bytes to read
   * @return true on success, false on failure
   */
  virtual bool read(uint32_t address, uint8_t *data, size_t length) = 0;

  /**
   * @brief Write data to storage
   *
   * Implementation handles device-specific requirements:
   * - FRAM: Direct write, any size
   * - EEPROM: Page-aligned writes with delays
   * - Flash: Requires erase before write
   *
   * @param address Starting address to write to
   * @param data Buffer containing data to write
   * @param length Number of bytes to write
   * @return true on success, false on failure
   */
  virtual bool write(uint32_t address, const uint8_t *data, size_t length) = 0;

  //========================================================================
  // Advanced Operations (Optional)
  //========================================================================

  /**
   * @brief Erase a block/sector (Flash devices only)
   *
   * For FRAM/EEPROM: This is a no-op (returns true)
   * For Flash: Erases the sector containing the address
   *
   * @param address Address within the block to erase
   * @return true on success, false on failure
   */
  virtual bool erase_block(uint32_t address) { return true; }

  /**
   * @brief Fill entire device with a value
   *
   * @param value Byte value to fill with (default: 0xFF for erase pattern)
   * @return Number of bytes written
   */
  virtual uint32_t fill(uint8_t value = 0xFF);

  /**
   * @brief Sync/flush any pending writes
   *
   * For FRAM: No-op (writes are instant)
   * For EEPROM: Wait for pending page write
   * For Flash: Ensure all writes are committed
   *
   * @return true on success, false on failure
   */
  virtual bool sync() { return true; }

  //========================================================================
  // LittleFS Block Device Interface
  //========================================================================

  /**
   * @brief Get block device configuration for LittleFS
   *
   * Provides optimal block sizes based on device characteristics
   *
   * @return Block device configuration
   */
  virtual BlockDeviceConfig get_block_config() const;

  /**
   * @brief Read a block for LittleFS
   *
   * @param block Block number
   * @param offset Offset within block
   * @param buffer Buffer to store data
   * @param size Number of bytes to read
   * @return 0 on success, negative on error
   */
  virtual int block_read(uint32_t block, uint32_t offset, void *buffer, uint32_t size);

  /**
   * @brief Program (write) a block for LittleFS
   *
   * @param block Block number
   * @param offset Offset within block
   * @param buffer Data to write
   * @param size Number of bytes to write
   * @return 0 on success, negative on error
   */
  virtual int block_prog(uint32_t block, uint32_t offset, const void *buffer, uint32_t size);

  /**
   * @brief Erase a block for LittleFS
   *
   * @param block Block number
   * @return 0 on success, negative on error
   */
  virtual int block_erase(uint32_t block);

  /**
   * @brief Sync block device for LittleFS
   *
   * @return 0 on success, negative on error
   */
  virtual int block_sync() { return this->sync() ? 0 : -1; }

 protected:
  bool is_valid_address_(uint32_t address, size_t length) const {
    return (address + length) <= this->get_capacity();
  }
};

}  // namespace binary_storage
}  // namespace esphome
