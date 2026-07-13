#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/storage/storage.h"
#include <cstdint>
#include <cstddef>

namespace esphome::binary_storage {

// Local path-buffer bound for the filesystem drivers in this component
// (the storage API itself only bounds names via storage::STORAGE_NAME_MAX).
static constexpr size_t STORAGE_MAX_PATH_LEN = 256;

#ifdef USE_BINARY_STORAGE_LITTLEFS
// Block device configuration for LittleFS integration.
// LittleFS requires block-oriented operations — this provides the translation
// layer between byte-addressable storage and LittleFS block callbacks.
struct BlockDeviceConfig {
  uint32_t block_size;
  uint32_t block_count;
  uint32_t read_size;
  uint32_t prog_size;
  uint32_t lookahead_size;
};
#endif  // USE_BINARY_STORAGE_LITTLEFS

// Abstract base for all binary storage devices (FRAM, EEPROM, SPI Flash, MRAM, OneWire EEPROM).
// Extends RawStorage — provides offset-based byte access.
class BinaryStorage : public storage::RawStorage {
 public:
  BinaryStorage() = default;

  // Component lifecycle
  void setup() override;
  void loop() override {}
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  //========================================================================
  // Device information (implement in each device driver)
  //========================================================================

  virtual uint32_t get_capacity() const = 0;
  virtual const char *get_device_name() const = 0;
  virtual const char *get_device_type() const = 0;
  virtual uint32_t get_page_size() const { return 1; }
  virtual uint32_t get_erase_size() const { return 0; }
  virtual bool is_ready() { return true; }

  // Deliberately explicit (not just the inherited default): every binary_storage device is an
  // external bus device (I2C/SPI/OneWire) and the bus is typically shared with components
  // driven from the main loop, so data-plane I/O must never run on the async worker task.
  // See StorageCaps in storage.h — do NOT add STORAGE_CAP_IO_TASK_SAFE here or in subclasses.
  uint8_t get_capabilities() const override { return 0; }

  //========================================================================
  // RawStorage interface (pure virtuals — implement in each device driver)
  //========================================================================

  storage::StorageError get_info(storage::StorageInfo *info) override;
  storage::StorageError read(uint64_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred) override = 0;
  storage::StorageError write(uint64_t offset, const uint8_t *buf, size_t len, size_t *bytes_transferred) override = 0;
  storage::StorageError erase(uint64_t offset, size_t len) override = 0;
  storage::StorageError format() override;

  //========================================================================
  // Utility
  //========================================================================

  // Fill entire device with a value. Returns bytes written.
  virtual uint32_t fill(uint8_t value);

#ifdef USE_BINARY_STORAGE_LITTLEFS
  //========================================================================
  // LittleFS block device interface (internal — used by LittleFSMount only)
  //========================================================================

  virtual BlockDeviceConfig get_block_config() const;
  virtual int block_read(uint32_t block, uint32_t offset, void *buffer, uint32_t size);
  virtual int block_prog(uint32_t block, uint32_t offset, const void *buffer, uint32_t size);
  virtual int block_erase(uint32_t block);
  virtual int block_sync() { return 0; }
#endif  // USE_BINARY_STORAGE_LITTLEFS

  //========================================================================
  // Configuration setters (called by Python codegen)
  //========================================================================

  void set_storage_id(const char *id) { this->storage_id_ = id; }
  void set_storage_name(const char *name) { this->storage_name_ = name; }

 protected:
  // Overflow-proof: valid iff [address, address+length) fits within capacity.
  bool is_valid_address_(uint64_t address, size_t length) const {
    const uint64_t cap = this->get_capacity();
    return length <= cap && address <= cap - length;
  }

  const char *storage_id_{nullptr};
  const char *storage_name_{nullptr};
};

}  // namespace esphome::binary_storage
