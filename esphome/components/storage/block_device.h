#pragma once

#include <cstdint>
#include <mutex>

#include "esphome/core/defines.h"
#include "esphome/core/log.h"

namespace esphome::storage {

enum class StorageError : int8_t {
  STORAGE_ERROR_NONE = 0,
  STORAGE_ERROR_IO = -1,
  STORAGE_ERROR_NO_MEDIA = -2,
  STORAGE_ERROR_READ_ONLY = -3,
  STORAGE_ERROR_INVALID = -4,
  STORAGE_ERROR_TIMEOUT = -5,
};

enum BlockFlag : uint8_t {
  BLOCK_FLAG_NEEDS_ERASE = 1 << 0,  // Programming can only clear bits; erase sets a block to 0xFF
  BLOCK_FLAG_REMOVABLE = 1 << 1,
  BLOCK_FLAG_READ_ONLY = 1 << 2,
  BLOCK_FLAG_TRIM = 1 << 3,
};

/// The shape of a block device. A block_count of 0 means no media is present.
struct BlockGeometry {
  uint32_t block_size{0};  // Erase unit in bytes
  uint32_t block_count{0};
  uint16_t read_size{1};     // Reads must be a multiple of this, at an offset that is a multiple of it
  uint16_t program_size{1};  // As read_size, for writes
  uint8_t flags{0};
};

const LogString *storage_error_to_string(StorageError error);

/// Stands in for a mutex where nothing runs on other tasks.
struct NullMutex {
  void lock() {}
  bool try_lock() { return true; }
  void unlock() {}
};

#ifdef USE_STORAGE_LOCK
using DeviceMutex = std::mutex;
#else
using DeviceMutex = NullMutex;
#endif

/** A device addressed in fixed size blocks, such as a flash chip, an SD card or a RAM buffer.
 *
 * Callers use the public methods, which check the address range, alignment and read-only state once,
 * so a driver only implements the transfer. An access may span several blocks.
 *
 * With USE_STORAGE_LOCK (a filesystem used from other tasks) each public call holds mutex(). A driver that touches
 * the device from its own loop() or timers must hold it too.
 */
class BlockDevice {
 public:
  StorageError read(uint32_t block, uint32_t offset, void *buf, uint32_t len);
  StorageError program(uint32_t block, uint32_t offset, const void *buf, uint32_t len);
  StorageError erase(uint32_t block, uint32_t count);
  /// Return once all earlier writes are stored on the media.
  StorageError sync();
  /// Tell the device that the blocks no longer hold data it needs to keep.
  StorageError trim(uint32_t block, uint32_t count);

  const BlockGeometry &get_geometry() const { return this->geometry_; }
  bool has_media() const { return this->geometry_.block_count != 0; }
  uint64_t get_capacity() const {
    return static_cast<uint64_t>(this->geometry_.block_size) * this->geometry_.block_count;
  }
  /// Changes each time media is inserted or removed, so a user can tell that its cached state is stale.
  uint8_t get_media_generation() const { return this->media_generation_; }
  DeviceMutex &mutex() { return this->mutex_; }

 protected:
  virtual StorageError device_read(uint32_t block, uint32_t offset, void *buf, uint32_t len) = 0;
  virtual StorageError device_program(uint32_t block, uint32_t offset, const void *buf, uint32_t len) = 0;
  virtual StorageError device_erase(uint32_t block, uint32_t count) { return StorageError::STORAGE_ERROR_NONE; }
  virtual StorageError device_sync() { return StorageError::STORAGE_ERROR_NONE; }
  virtual StorageError device_trim(uint32_t block, uint32_t count) { return StorageError::STORAGE_ERROR_NONE; }

  void media_changed_() { this->media_generation_++; }
  void dump_geometry_(const char *tag) const;

  StorageError check_access_(uint32_t block, uint32_t offset, uint32_t len, uint16_t align) const;
  StorageError check_range_(uint32_t block, uint32_t count) const;

  BlockGeometry geometry_{};
  DeviceMutex mutex_;
  uint8_t media_generation_{0};
};

}  // namespace esphome::storage
