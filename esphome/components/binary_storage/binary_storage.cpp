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

  // Backing-only devices do not register: that keeps them out of the raw API, the storages
  // listing and the device nodes.
  if (this->raw_enabled_ && storage::global_storage_registry != nullptr)
    storage::global_storage_registry->register_storage(this);
}

void BinaryStorage::dump_config() {
  ESP_LOGCONFIG(TAG, "Binary Storage:");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->get_device_name());
  ESP_LOGCONFIG(TAG, "  Capacity: %" PRIu32 " bytes", this->get_capacity());
}

storage::StorageError BinaryStorage::get_info(storage::StorageInfo *info) {
  if (info == nullptr)
    return storage::StorageError::STORAGE_ERROR_INVALID_ARGS;

  info->id = this->storage_id_ != nullptr ? this->storage_id_ : this->get_device_type();
  info->name = this->storage_name_ != nullptr ? this->storage_name_ : this->get_device_name();
  info->kind = this->get_device_type();          // "eeprom", "fram", ... -- already the driver's own kind string
  info->total_bytes = this->get_raw_capacity();  // the raw side's world ends at the window
  info->free_bytes = 0;
  info->block_size = this->get_page_size();
  info->is_mounted = true;
  info->is_removable = false;
  info->is_read_only = false;

  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError BinaryStorage::format() {
  // format() is a RawStorage operation: it blanks the raw window and must never touch the
  // filesystem region below it -- fill() goes through the window wrappers, so it cannot.
  uint64_t raw_cap = this->get_raw_capacity();
  uint32_t written = this->fill(0xFF);
  return (written == raw_cap) ? storage::StorageError::STORAGE_ERROR_OK
                              : storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
}

// ===========================================================================================
// The raw window. Raw address 0 is physical raw_offset_: the raw side
// is a genuinely separate memory whose capacity is what the filesystem left over. Anything
// outside is INVALID_ARGS here, before a driver ever sees it -- the split cannot be crossed.
// With no reservation these are transparent (offset + 0, full capacity).

storage::StorageError BinaryStorage::read(uint64_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred) {
  const uint64_t cap = this->get_raw_capacity();
  if (len > cap || offset > cap - len)
    return storage::StorageError::STORAGE_ERROR_INVALID_ARGS;
  return this->read_physical(offset + this->raw_offset_, buf, len, bytes_transferred);
}

storage::StorageError BinaryStorage::write(uint64_t offset, const uint8_t *buf, size_t len, size_t *bytes_transferred) {
  const uint64_t cap = this->get_raw_capacity();
  if (len > cap || offset > cap - len)
    return storage::StorageError::STORAGE_ERROR_INVALID_ARGS;
  return this->write_physical(offset + this->raw_offset_, buf, len, bytes_transferred);
}

storage::StorageError BinaryStorage::erase(uint64_t offset, size_t len) {
  const uint64_t cap = this->get_raw_capacity();
  if (len > cap || offset > cap - len)
    return storage::StorageError::STORAGE_ERROR_INVALID_ARGS;
  // the raw window is validated (setup()), so rebasing keeps
  // the caller's alignment intact -- the driver's own alignment checks still apply physically.
  return this->erase_physical(offset + this->raw_offset_, len);
}

void BinaryStorage::get_raw_geometry(storage::RawGeometry *out) const {
  out->capacity = this->get_raw_capacity();  // mode: both -- the FS region is not raw's to see
  out->write_page = this->get_page_size();
  out->erase_sector = this->get_erase_size();
  out->erase_block = this->get_erase_block_size();
  out->caps = this->get_erase_caps();
}

uint32_t BinaryStorage::fill(uint8_t value) {
  const size_t buffer_size = std::min(static_cast<uint32_t>(256u), this->get_page_size() * 4);
  uint8_t buffer[buffer_size];
  std::fill(buffer, buffer + buffer_size, value);

  uint32_t address = 0;
  // fill() addresses the raw window (writes below go through the rebasing wrapper).
  auto capacity = static_cast<uint32_t>(this->get_raw_capacity());

  while (address < capacity) {
    size_t chunk_size = std::min((size_t) (capacity - address), buffer_size);
    if (this->write(address, buffer, chunk_size, nullptr) != storage::StorageError::STORAGE_ERROR_OK) {
      ESP_LOGE(TAG, "Fill failed at address 0x%" PRIx32, address);
      return address;
    }
    address += chunk_size;
  }

  return capacity;
}

}  // namespace esphome::binary_storage
