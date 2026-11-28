#include "block_device.h"

#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/progmem.h"

namespace esphome::storage {

ESPHOME_LOG_TAG(TAG, "storage");

// Indexed by the negated StorageError value; the last entry is the fallback
PROGMEM_STRING_TABLE(StorageErrorStrings, "none", "I/O error", "no media", "read only", "invalid address or size",
                     "timeout", "unknown");

const LogString *storage_error_to_string(StorageError error) {
  return StorageErrorStrings::get_log_str(-static_cast<int>(error));
}

StorageError BlockDevice::check_access_(uint32_t block, uint32_t offset, uint32_t len, uint16_t align,
                                        bool write) const {
  if (!this->has_media())
    return StorageError::STORAGE_ERROR_NO_MEDIA;
  if (write && (this->geometry_.flags & BLOCK_FLAG_READ_ONLY))
    return StorageError::STORAGE_ERROR_READ_ONLY;
  if (offset % align != 0 || len % align != 0)
    return StorageError::STORAGE_ERROR_INVALID;
  const uint64_t start = static_cast<uint64_t>(block) * this->geometry_.block_size + offset;
  if (block >= this->geometry_.block_count || start + len > this->get_capacity())
    return StorageError::STORAGE_ERROR_INVALID;
  return StorageError::STORAGE_ERROR_NONE;
}

StorageError BlockDevice::check_range_(uint32_t block, uint32_t count) const {
  if (!this->has_media())
    return StorageError::STORAGE_ERROR_NO_MEDIA;
  if (this->geometry_.flags & BLOCK_FLAG_READ_ONLY)
    return StorageError::STORAGE_ERROR_READ_ONLY;
  if (count == 0 || block >= this->geometry_.block_count || count > this->geometry_.block_count - block)
    return StorageError::STORAGE_ERROR_INVALID;
  return StorageError::STORAGE_ERROR_NONE;
}

StorageError BlockDevice::read(uint32_t block, uint32_t offset, void *buf, uint32_t len) {
  // A single filesystem call, such as a format, can make thousands of transfers. Only a task the watchdog watches
  // feeds it: the main loop, also during setup(), but not a task using a filesystem through the C library. The feed
  // can run other code, such as a status LED, so it comes before the lock.
  App.feed_wdt_if_watched();
  const std::scoped_lock lock{this->mutex_};
  if (len == 0)
    return StorageError::STORAGE_ERROR_NONE;
  StorageError err = this->check_access_(block, offset, len, this->geometry_.read_size, false);
  if (err != StorageError::STORAGE_ERROR_NONE)
    return err;
  return this->device_read(block, offset, buf, len);
}

StorageError BlockDevice::program(uint32_t block, uint32_t offset, const void *buf, uint32_t len) {
  App.feed_wdt_if_watched();
  const std::scoped_lock lock{this->mutex_};
  if (len == 0)
    return StorageError::STORAGE_ERROR_NONE;
  StorageError err = this->check_access_(block, offset, len, this->geometry_.program_size, true);
  if (err != StorageError::STORAGE_ERROR_NONE)
    return err;
  return this->device_program(block, offset, buf, len);
}

StorageError BlockDevice::erase(uint32_t block, uint32_t count) {
  App.feed_wdt_if_watched();
  const std::scoped_lock lock{this->mutex_};
  StorageError err = this->check_range_(block, count);
  if (err != StorageError::STORAGE_ERROR_NONE)
    return err;
  return this->device_erase(block, count);
}

StorageError BlockDevice::sync() {
  const std::scoped_lock lock{this->mutex_};
  if (!this->has_media())
    return StorageError::STORAGE_ERROR_NO_MEDIA;
  return this->device_sync();
}

StorageError BlockDevice::trim(uint32_t block, uint32_t count) {
  const std::scoped_lock lock{this->mutex_};
  StorageError err = this->check_range_(block, count);
  if (err != StorageError::STORAGE_ERROR_NONE || !(this->geometry_.flags & BLOCK_FLAG_TRIM))
    return err;
  return this->device_trim(block, count);
}

void BlockDevice::dump_geometry_(const char *tag) const {
  if (!this->has_media()) {
    ESP_LOGCONFIG(tag, "  No media");
    return;
  }
  const BlockGeometry &g = this->geometry_;
  ESP_LOGCONFIG(tag,
                "  Capacity: %" PRIu32 " KiB\n"
                "  Block size: %" PRIu32 "\n"
                "  Block count: %" PRIu32 "\n"
                "  Read size: %u\n"
                "  Program size: %u\n"
                "  Needs erase: %s\n"
                "  Removable: %s\n"
                "  Read only: %s",
                static_cast<uint32_t>(this->get_capacity() / 1024), g.block_size, g.block_count, g.read_size,
                g.program_size, YESNO(g.flags & BLOCK_FLAG_NEEDS_ERASE), YESNO(g.flags & BLOCK_FLAG_REMOVABLE),
                YESNO(g.flags & BLOCK_FLAG_READ_ONLY));
}

}  // namespace esphome::storage
