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

  // mode: both contract -- LittleFS owns [0, fs_reserved_), raw the rest. Capacity may only
  // be known here (model autodetect), so the split is validated at runtime, loudly.
  if (this->fs_reserved_ > 0) {
    if (this->fs_reserved_ >= this->get_capacity()) {
      ESP_LOGE(TAG, "fs_size (%" PRIu32 ") swallows the whole device (%" PRIu32 " bytes) -- nothing left for raw",
               this->fs_reserved_, this->get_capacity());
      this->mark_failed();
      return;
    }
    uint32_t erase_size = this->get_erase_size();
    if (erase_size > 0 && this->fs_reserved_ % erase_size != 0) {
      ESP_LOGE(TAG, "fs_size (%" PRIu32 ") is not a multiple of the erase sector (%" PRIu32 ")", this->fs_reserved_,
               erase_size);
      this->mark_failed();
      return;
    }
    ESP_LOGCONFIG(TAG, "  Split: LittleFS [0, %" PRIu32 "), raw %" PRIu64 " bytes above it", this->fs_reserved_,
                  this->get_raw_capacity());
  }

  // mode: littlefs -- a filesystem backing only. Not registering is what keeps the device out
  // of the raw API, the storages listing and the device nodes; the mount holds its own
  // pointer and is unaffected.
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
    return storage::StorageError::INVALID_ARGS;

  info->id = this->storage_id_ != nullptr ? this->storage_id_ : this->get_device_type();
  info->name = this->storage_name_ != nullptr ? this->storage_name_ : this->get_device_name();
  info->kind = this->get_device_type();          // "eeprom", "fram", ... -- already the driver's own kind string
  info->total_bytes = this->get_raw_capacity();  // the raw side's world ends at the window
  info->free_bytes = 0;
  info->block_size = this->get_page_size();
  info->is_mounted = true;
  info->is_removable = false;
  info->is_read_only = false;

  return storage::StorageError::OK;
}

storage::StorageError BinaryStorage::format() {
  // format() is a RawStorage operation: it blanks the raw window and must never touch the
  // filesystem region below it -- fill() goes through the window wrappers, so it cannot.
  uint64_t raw_cap = this->get_raw_capacity();
  uint32_t written = this->fill(0xFF);
  return (written == raw_cap) ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

// ===========================================================================================
// The raw window (mode: both contract). Raw address 0 is physical fs_reserved_: the raw side
// is a genuinely separate memory whose capacity is what the filesystem left over. Anything
// outside is INVALID_ARGS here, before a driver ever sees it -- the split cannot be crossed.
// With no reservation these are transparent (offset + 0, full capacity).

storage::StorageError BinaryStorage::read(uint64_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred) {
  const uint64_t cap = this->get_raw_capacity();
  if (len > cap || offset > cap - len)
    return storage::StorageError::INVALID_ARGS;
  return this->read_physical(offset + this->fs_reserved_, buf, len, bytes_transferred);
}

storage::StorageError BinaryStorage::write(uint64_t offset, const uint8_t *buf, size_t len, size_t *bytes_transferred) {
  const uint64_t cap = this->get_raw_capacity();
  if (len > cap || offset > cap - len)
    return storage::StorageError::INVALID_ARGS;
  return this->write_physical(offset + this->fs_reserved_, buf, len, bytes_transferred);
}

storage::StorageError BinaryStorage::erase(uint64_t offset, size_t len) {
  const uint64_t cap = this->get_raw_capacity();
  if (len > cap || offset > cap - len)
    return storage::StorageError::INVALID_ARGS;
  // fs_reserved_ is validated (setup()) as a multiple of the erase sector, so rebasing keeps
  // the caller's alignment intact -- the driver's own alignment checks still apply physically.
  return this->erase_physical(offset + this->fs_reserved_, len);
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

  // mode: both -- the filesystem owns exactly [0, fs_reserved_); everything above is raw's.
  uint32_t fs_extent = this->fs_reserved_ > 0 ? this->fs_reserved_ : capacity;
  config.block_count = fs_extent / config.block_size;
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

  // Physical, deliberately: the FS region sits below the raw window, which would refuse it.
  return (this->read_physical(address, static_cast<uint8_t *>(buffer), size, nullptr) == storage::StorageError::OK)
             ? 0
             : -1;
}

int BinaryStorage::block_prog(uint32_t block, uint32_t offset, const void *buffer, uint32_t size) {
  BlockDeviceConfig cfg = this->get_block_config();
  uint32_t address = block * cfg.block_size + offset;

  if (!this->is_valid_address_(address, size)) {
    ESP_LOGE(TAG, "Block program out of bounds: block=%" PRIu32 ", offset=%" PRIu32 ", size=%" PRIu32, block, offset,
             size);
    return -1;
  }

  return (this->write_physical(address, static_cast<const uint8_t *>(buffer), size, nullptr) ==
          storage::StorageError::OK)
             ? 0
             : -1;
}

int BinaryStorage::block_erase(uint32_t block) {
  BlockDeviceConfig cfg = this->get_block_config();
  uint32_t address = block * cfg.block_size;

  // littlefs calls this before programming a block. On media that overwrite in place there is
  // nothing to do, and "nothing to do" is success for a block device -- a different question
  // from erase() below, which must not claim a range was blanked when the medium cannot.
  if (this->get_erase_caps() == 0)
    return 0;

  return (this->erase_physical(address, cfg.block_size) == storage::StorageError::OK) ? 0 : -1;
}

#endif  // USE_BINARY_STORAGE_LITTLEFS

}  // namespace esphome::binary_storage
