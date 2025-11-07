#include "binary_storage.h"
#include "esphome/core/log.h"
#include <algorithm>

// Forward declare storage_host for soft dependency
#if defined(USE_STORAGE_HOST)
namespace storage_host {
extern class StorageHost *global_storage_host;
}
#endif  // USE_STORAGE_HOST

namespace esphome {
namespace binary_storage {

void BinaryStorage::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Binary Storage...");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->get_device_name());
  ESP_LOGCONFIG(TAG, "  Type: %s", this->get_device_type());
  ESP_LOGCONFIG(TAG, "  Capacity: %u bytes (%.1f KB)", this->get_capacity(), this->get_capacity() / 1024.0f);
  ESP_LOGCONFIG(TAG, "  Page Size: %u bytes", this->get_page_size());

  uint32_t erase_size = this->get_erase_size();
  if (erase_size > 0) {
    ESP_LOGCONFIG(TAG, "  Erase Block Size: %u bytes", erase_size);
  }
}

void BinaryStorage::dump_config() {
  ESP_LOGCONFIG(TAG, "Binary Storage:");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->get_device_name());
  ESP_LOGCONFIG(TAG, "  Capacity: %u bytes", this->get_capacity());
}

void BinaryStorage::register_with_storage_host(const std::string &device_node_path) {
#if defined(USE_STORAGE_HOST)
  // Check if storage_host is available (soft dependency)
  if (storage_host::global_storage_host != nullptr) {
    storage_host::global_storage_host->register_device_node(device_node_path, this, this->get_device_type());
    ESP_LOGI(TAG, "Registered device node: %s -> %s", device_node_path.c_str(), this->get_device_name());
  } else {
    ESP_LOGD(TAG, "storage_host not available, skipping device node registration");
  }
#else
  ESP_LOGD(TAG, "storage_host component not compiled, device node registration disabled");
#endif  // USE_STORAGE_HOST
}

uint32_t BinaryStorage::fill(uint8_t value) {
  const size_t buffer_size = std::min(256u, this->get_page_size() * 4);
  uint8_t buffer[buffer_size];
  std::fill(buffer, buffer + buffer_size, value);

  uint32_t address = 0;
  uint32_t capacity = this->get_capacity();

  while (address < capacity) {
    size_t chunk_size = std::min((size_t)(capacity - address), buffer_size);
    if (!this->write(address, buffer, chunk_size)) {
      ESP_LOGE(TAG, "Fill failed at address 0x%X", address);
      return address;
    }
    address += chunk_size;
  }

  return capacity;
}

BlockDeviceConfig BinaryStorage::get_block_config() const {
  BlockDeviceConfig config;

  // Determine optimal block size based on device characteristics
  uint32_t page_size = this->get_page_size();
  uint32_t erase_size = this->get_erase_size();
  uint32_t capacity = this->get_capacity();

  // For Flash devices, use erase size as block size
  if (erase_size > 0) {
    config.block_size = erase_size;
  }
  // For EEPROM/FRAM, use a reasonable block size (4KB is common)
  else {
    config.block_size = 4096;
    // But don't exceed capacity
    if (config.block_size > capacity) {
      config.block_size = capacity;
    }
  }

  config.block_count = capacity / config.block_size;
  config.read_size = 1;  // Can read single bytes
  config.prog_size = page_size > 0 ? page_size : 1;  // Program size is page size
  config.lookahead_size = (config.block_count + 7) / 8;  // 1 bit per block

  return config;
}

int BinaryStorage::block_read(uint32_t block, uint32_t offset, void *buffer, uint32_t size) {
  BlockDeviceConfig cfg = this->get_block_config();
  uint32_t address = block * cfg.block_size + offset;

  if (!this->is_valid_address_(address, size)) {
    ESP_LOGE(TAG, "Block read out of bounds: block=%u, offset=%u, size=%u", block, offset, size);
    return -1;
  }

  return this->read(address, static_cast<uint8_t *>(buffer), size) ? 0 : -1;
}

int BinaryStorage::block_prog(uint32_t block, uint32_t offset, const void *buffer, uint32_t size) {
  BlockDeviceConfig cfg = this->get_block_config();
  uint32_t address = block * cfg.block_size + offset;

  if (!this->is_valid_address_(address, size)) {
    ESP_LOGE(TAG, "Block program out of bounds: block=%u, offset=%u, size=%u", block, offset, size);
    return -1;
  }

  return this->write(address, static_cast<const uint8_t *>(buffer), size) ? 0 : -1;
}

int BinaryStorage::block_erase(uint32_t block) {
  BlockDeviceConfig cfg = this->get_block_config();
  uint32_t address = block * cfg.block_size;

  return this->erase_block(address) ? 0 : -1;
}

}  // namespace binary_storage
}  // namespace esphome
