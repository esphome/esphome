#include "binary_storage.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#include "esphome/components/storage/storage.h"
#include <algorithm>
#include <cstring>

namespace esphome::binary_storage {

static const char *const TAG = "binary_storage";

void BinaryStorage::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Binary Storage...");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->get_device_name());
  ESP_LOGCONFIG(TAG, "  Type: %s", this->get_device_type());
  ESP_LOGCONFIG(TAG, "  Capacity: %" PRIu32 " bytes (%.1f KB)", this->get_capacity(), this->get_capacity() / 1024.0f);
  ESP_LOGCONFIG(TAG, "  Page Size: %" PRIu32 " bytes", this->get_page_size());

  uint32_t erase_size = this->get_erase_size();
  if (erase_size > 0) {
    ESP_LOGCONFIG(TAG, "  Erase Block Size: %" PRIu32 " bytes", erase_size);
  }

  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->register_storage(this);
}

void BinaryStorage::dump_config() {
  ESP_LOGCONFIG(TAG, "Binary Storage:");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->get_device_name());
  ESP_LOGCONFIG(TAG, "  Capacity: %" PRIu32 " bytes", this->get_capacity());
}

storage::StorageError BinaryStorage::get_info(storage::StorageInfo *info) {
  if (info == nullptr)
    return storage::StorageError::INVALID_ARGS;

  info->id = this->storage_id_ != nullptr ? this->storage_id_ : this->get_device_type();
  info->name = this->storage_name_ != nullptr ? this->storage_name_ : this->get_device_name();
  info->total_bytes = this->get_capacity();
  info->free_bytes = 0;
  info->block_size = this->get_page_size();
  info->is_mounted = true;
  info->is_removable = false;
  info->is_read_only = false;

  return storage::StorageError::OK;
}

storage::StorageError BinaryStorage::format() {
  uint32_t written = this->fill(0xFF);
  return (written == this->get_capacity()) ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

uint32_t BinaryStorage::fill(uint8_t value) {
  const size_t buffer_size = std::min(static_cast<uint32_t>(256u), this->get_page_size() * 4);
  uint8_t buffer[buffer_size];
  std::fill(buffer, buffer + buffer_size, value);

  uint32_t address = 0;
  uint32_t capacity = this->get_capacity();

  while (address < capacity) {
    size_t chunk_size = std::min((size_t) (capacity - address), buffer_size);
    if (this->write(address, buffer, chunk_size, nullptr) != storage::StorageError::OK) {
      ESP_LOGE(TAG, "Fill failed at address 0x%" PRIx32, address);
      return address;
    }
    address += chunk_size;
  }

  return capacity;
}

#ifdef USE_BINARY_STORAGE_LITTLEFS

BlockDeviceConfig BinaryStorage::get_block_config() const {
  BlockDeviceConfig config;

  uint32_t page_size = this->get_page_size();
  uint32_t erase_size = this->get_erase_size();
  uint32_t capacity = this->get_capacity();

  if (erase_size > 0) {
    config.block_size = erase_size;
  } else {
    config.block_size = 4096;
    if (config.block_size > capacity) {
      config.block_size = capacity;
    }
  }

  config.block_count = capacity / config.block_size;
  config.read_size = 1;
  config.prog_size = page_size > 0 ? page_size : 1;

  uint32_t lookahead_bytes = (config.block_count + 7) / 8;
  config.lookahead_size = ((lookahead_bytes + 7) / 8) * 8;
  if (config.lookahead_size == 0) {
    config.lookahead_size = 8;
  }

  return config;
}

int BinaryStorage::block_read(uint32_t block, uint32_t offset, void *buffer, uint32_t size) {
  BlockDeviceConfig cfg = this->get_block_config();
  uint32_t address = block * cfg.block_size + offset;

  if (!this->is_valid_address_(address, size)) {
    ESP_LOGE(TAG, "Block read out of bounds: block=%" PRIu32 ", offset=%" PRIu32 ", size=%" PRIu32, block, offset,
             size);
    return -1;
  }

  return (this->read(address, static_cast<uint8_t *>(buffer), size, nullptr) == storage::StorageError::OK) ? 0 : -1;
}

int BinaryStorage::block_prog(uint32_t block, uint32_t offset, const void *buffer, uint32_t size) {
  BlockDeviceConfig cfg = this->get_block_config();
  uint32_t address = block * cfg.block_size + offset;

  if (!this->is_valid_address_(address, size)) {
    ESP_LOGE(TAG, "Block program out of bounds: block=%" PRIu32 ", offset=%" PRIu32 ", size=%" PRIu32, block, offset,
             size);
    return -1;
  }

  return (this->write(address, static_cast<const uint8_t *>(buffer), size, nullptr) == storage::StorageError::OK) ? 0
                                                                                                                  : -1;
}

int BinaryStorage::block_erase(uint32_t block) {
  BlockDeviceConfig cfg = this->get_block_config();
  uint32_t address = block * cfg.block_size;

  return (this->erase(address, cfg.block_size) == storage::StorageError::OK) ? 0 : -1;
}

#endif  // USE_BINARY_STORAGE_LITTLEFS

}  // namespace esphome::binary_storage
