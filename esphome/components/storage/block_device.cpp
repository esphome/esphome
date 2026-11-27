#include "block_device.h"

#include <cinttypes>

#include "esphome/core/application.h"

namespace esphome::storage {

ESPHOME_LOG_TAG(TAG, "storage");

const LogString *storage_error_to_string(StorageError error) {
  switch (error) {
    case StorageError::STORAGE_ERROR_NONE:
      return LOG_STR("none");
    case StorageError::STORAGE_ERROR_IO:
      return LOG_STR("I/O error");
    case StorageError::STORAGE_ERROR_NO_MEDIA:
      return LOG_STR("no media");
    case StorageError::STORAGE_ERROR_READ_ONLY:
      return LOG_STR("read only");
    case StorageError::STORAGE_ERROR_INVALID:
      return LOG_STR("invalid address or size");
    case StorageError::STORAGE_ERROR_TIMEOUT:
      return LOG_STR("timeout");
  }
  return LOG_STR("unknown");
}

StorageError BlockDevice::check_access_(uint32_t block, uint32_t offset, uint32_t len, uint16_t align) const {
  if (!this->has_media())
    return StorageError::STORAGE_ERROR_NO_MEDIA;
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
  if (count == 0 || block >= this->geometry_.block_count || count > this->geometry_.block_count - block)
    return StorageError::STORAGE_ERROR_INVALID;
  return StorageError::STORAGE_ERROR_NONE;
}

StorageError BlockDevice::read(uint32_t block, uint32_t offset, void *buf, uint32_t len) {
  const std::scoped_lock lock{this->mutex_};
  if (len == 0)
    return StorageError::STORAGE_ERROR_NONE;
  StorageError err = this->check_access_(block, offset, len, this->geometry_.read_size);
  if (err != StorageError::STORAGE_ERROR_NONE)
    return err;
  // A single filesystem call, such as a format, can make thousands of transfers. Another task (a filesystem used
  // through the C library) must not feed it: that would hold back the main loop's own feed.
  if (is_main_loop_thread())
    App.feed_wdt();
  return this->device_read(block, offset, buf, len);
}

StorageError BlockDevice::program(uint32_t block, uint32_t offset, const void *buf, uint32_t len) {
  const std::scoped_lock lock{this->mutex_};
  if (this->geometry_.flags & BLOCK_FLAG_READ_ONLY)
    return StorageError::STORAGE_ERROR_READ_ONLY;
  if (len == 0)
    return StorageError::STORAGE_ERROR_NONE;
  StorageError err = this->check_access_(block, offset, len, this->geometry_.program_size);
  if (err != StorageError::STORAGE_ERROR_NONE)
    return err;
  if (is_main_loop_thread())
    App.feed_wdt();
  return this->device_program(block, offset, buf, len);
}

StorageError BlockDevice::erase(uint32_t block, uint32_t count) {
  const std::scoped_lock lock{this->mutex_};
  if (this->geometry_.flags & BLOCK_FLAG_READ_ONLY)
    return StorageError::STORAGE_ERROR_READ_ONLY;
  StorageError err = this->check_range_(block, count);
  if (err != StorageError::STORAGE_ERROR_NONE)
    return err;
  if (is_main_loop_thread())
    App.feed_wdt();
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
  if (this->geometry_.flags & BLOCK_FLAG_READ_ONLY)
    return StorageError::STORAGE_ERROR_READ_ONLY;
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
