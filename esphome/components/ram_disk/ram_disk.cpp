#include "ram_disk.h"

#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ram_disk {

ESPHOME_LOG_TAG(TAG, "ram_disk");

using storage::StorageError;

void RamDisk::setup() {
  const size_t size = static_cast<size_t>(this->geometry_.block_size) * this->block_count_;
  RAMAllocator<uint8_t> allocator;
  this->data_ = allocator.allocate(size);
  if (this->data_ == nullptr) {
    this->mark_failed(LOG_STR("Not enough memory"));
    return;
  }
  memset(this->data_, 0, size);
  this->geometry_.block_count = this->block_count_;
  this->media_changed_();
}

void RamDisk::dump_config() {
  ESP_LOGCONFIG(TAG, "RAM disk:");
  this->dump_geometry_(TAG);
}

StorageError RamDisk::device_read(uint32_t block, uint32_t offset, void *buf, uint32_t len) {
  memcpy(buf, this->data_ + static_cast<size_t>(block) * this->geometry_.block_size + offset, len);
  return StorageError::STORAGE_ERROR_NONE;
}

StorageError RamDisk::device_program(uint32_t block, uint32_t offset, const void *buf, uint32_t len) {
  memcpy(this->data_ + static_cast<size_t>(block) * this->geometry_.block_size + offset, buf, len);
  return StorageError::STORAGE_ERROR_NONE;
}

}  // namespace esphome::ram_disk
