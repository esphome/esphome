#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/storage/storage.h"
#include <cstdint>
#include <cstddef>

namespace esphome::binary_storage {

// Full VFS path buffer for the filesystem drivers in this component: the storage API's bound
// for the relative path, plus room for the mount point it gets prefixed with (ESP_VFS_PATH_MAX
// is 15). Derived so it cannot drift from what the API hands down.
static constexpr size_t STORAGE_MAX_PATH_LEN = storage::STORAGE_PATH_MAX + 32;

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
  // Larger erase unit, 0 when the device has none (see RawEraseCaps::RAW_ERASE_BLOCK).
  virtual uint32_t get_erase_block_size() const { return 0; }
  // Default: media that overwrite in place (FRAM, EEPROM) — no erase, none needed.
  virtual uint8_t get_erase_caps() const { return 0; }
  virtual bool is_ready() { return true; }

  // Task-safety of a raw device is a property of its BUS, not the driver: every
  // binary_storage device is an external bus device (I2C/SPI/OneWire), and if that bus is
  // shared with components driven from the main loop, running data-plane I/O on the async
  // worker task would race their access and corrupt the bus. So the default is 0 (loop-sliced
  // only) and that is the safe choice.
  //
  // A user who KNOWS this device is alone on its bus can opt in with assume_exclusive_bus (see
  // set_assume_exclusive_bus / the config key). Only then — and only on a platform that
  // actually has the background worker task — do we advertise STORAGE_CAP_IO_TASK_SAFE. This
  // is a promise about hardware the driver cannot verify; a wrong promise corrupts the bus.
  // See .ai/architecture/task-safe-raw-devices.md for the full contract.
  uint8_t get_capabilities() const override {
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
    if (this->assume_exclusive_bus_)
      return storage::StorageCaps::STORAGE_CAP_IO_TASK_SAFE;
#endif
    return 0;
  }

  //========================================================================
  // RawStorage interface (pure virtuals — implement in each device driver)
  //========================================================================

  storage::StorageError get_info(storage::StorageInfo *info) override;
  // The raw window (mode: both contract — see set_fs_reserved()): these final wrappers
  // translate raw addresses into physical ones and refuse anything outside the window, then
  // delegate to the drivers' *_physical_() below. Drivers cannot bypass the contract.
  storage::StorageError read(uint64_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred) final;
  storage::StorageError write(uint64_t offset, const uint8_t *buf, size_t len, size_t *bytes_transferred) final;
  storage::StorageError erase(uint64_t offset, size_t len) final;
  storage::StorageError format() override;
  // Implemented once here from the device getters above — drivers only report their numbers.
  void get_raw_geometry(storage::RawGeometry *out) const override;

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
  // mode: both — the split contract: LittleFS owns [0, fs_size), raw the rest. Raw addresses
  // are rebased (raw 0 = fs_size physically) so the two are genuinely separate memories: the
  // raw side reports capacity - fs_size and no raw operation can reach the filesystem.
  void set_fs_reserved(uint32_t bytes) { this->fs_reserved_ = bytes; }
  // mode: littlefs — the device is a filesystem backing only: it never registers as a raw
  // storage, so it has no raw API presence, no device node, no automations target.
  void set_raw_enabled(bool enabled) { this->raw_enabled_ = enabled; }
  // Opt-in: the user asserts this device is alone on its bus, so its data-plane I/O may run on
  // the async worker task (see get_capabilities above and the contract in .ai/). Off by
  // default; only has an effect on a platform with the worker task.
  void set_assume_exclusive_bus(bool assume) { this->assume_exclusive_bus_ = assume; }
  // What the raw side may use. 0 when raw is disabled or the FS reservation swallows
  // everything (the latter is a config error caught in setup()).
  uint64_t get_raw_capacity() const {
    if (!this->raw_enabled_)
      return 0;
    const uint64_t cap = this->get_capacity();
    return this->fs_reserved_ < cap ? cap - this->fs_reserved_ : 0;
  }
#ifdef USE_STORAGE_DEVICE_NODES
  void set_device_node_name(const char *name) { this->device_node_name_ = name; }
  bool has_device_node() const override { return this->device_node_name_ != nullptr; }
  const char *get_device_node_name() const override { return this->device_node_name_; }
#endif

 protected:
  // The physical device operations — implemented by each driver, addresses are device
  // addresses over the full capacity. Only the window wrappers above and the LittleFS block
  // callbacks (which must reach [0, fs_reserved_)) call these.
  virtual storage::StorageError read_physical_(uint64_t offset, uint8_t *buf, size_t len,
                                               size_t *bytes_transferred) = 0;
  virtual storage::StorageError write_physical_(uint64_t offset, const uint8_t *buf, size_t len,
                                                size_t *bytes_transferred) = 0;
  virtual storage::StorageError erase_physical_(uint64_t offset, size_t len) = 0;

  // Overflow-proof: valid iff [address, address+length) fits within capacity.
  bool is_valid_address_(uint64_t address, size_t length) const {
    const uint64_t cap = this->get_capacity();
    return length <= cap && address <= cap - length;
  }

  const char *storage_id_{nullptr};
  const char *storage_name_{nullptr};
  uint32_t fs_reserved_{0};           // bytes at the bottom owned by LittleFS (mode: both), 0 = none
  bool raw_enabled_{true};            // false for mode: littlefs — no raw registration or window
  bool assume_exclusive_bus_{false};  // opt-in: device is alone on its bus → task-safe I/O
#ifdef USE_STORAGE_DEVICE_NODES
  // nullptr = no node for this device (device_node: false, or no browser configured at all).
  const char *device_node_name_{nullptr};
#endif
};

}  // namespace esphome::binary_storage
