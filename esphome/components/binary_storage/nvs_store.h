#pragma once

#include "esphome/core/defines.h"

#if defined(USE_BINARY_STORAGE_NVS) || defined(USE_ESP32_PREFERENCES_STORAGE)

#include "esphome/core/component.h"
#include "esphome/components/storage/storage.h"

#include <nvs.h>

namespace esphome {
namespace binary_storage {

// A KeyValueStorage backed by a dedicated ESP-IDF NVS partition. NVS is the canonical native
// key-value store on esp32, so it is the natural reference backend for the KeyValueStorage
// interface. This uses its OWN partition (partition_label) -- it does NOT touch the system "nvs"
// partition that ESPHome preferences use, so the two never collide.
//
// The uint32 key maps to NVS' string key via decimal formatting, matching esp32 preferences'
// uint32_to_str(), so a future preferences adoption could share this backend's namespace.
class NVSStore : public storage::KeyValueStorage {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_partition_label(const char *label) { this->partition_label_ = label; }
  void set_namespace(const char *ns) { this->namespace_ = ns; }
  void set_storage_id(const char *id) { this->storage_id_ = id; }
  void set_storage_name(const char *name) { this->storage_name_ = name; }

  // Adopt an already-open NVS handle instead of opening one. The store then skips its own init/open
  // and issues get/set/erase against the caller's handle. Used by the esp32 preferences path, which
  // must open the system "esphome" namespace itself very early (before the logger) and only wants
  // this class as the KeyValueStorage view over that handle.
  void adopt_handle(nvs_handle_t handle) {
    this->handle_ = handle;
    this->opened_ = true;
    this->initialized_ = true;
  }

  storage::StorageError get_info(storage::StorageInfo *info) override;

  storage::StorageError get(uint32_t key, uint8_t *buf, size_t len, size_t *got) override;
  storage::StorageError set(uint32_t key, const uint8_t *data, size_t len) override;
  storage::StorageError erase(uint32_t key) override;
  bool has(uint32_t key) override;
  storage::StorageError get_size(uint32_t key, size_t *out) override;
  storage::StorageError ensure_initialized() override;
  storage::StorageError format() override;

 protected:
  bool open_();  // lazily open the nvs handle after ensure_initialized()

  const char *partition_label_{nullptr};
  const char *namespace_{"binary_storage"};
  const char *storage_id_{nullptr};
  const char *storage_name_{nullptr};

  nvs_handle_t handle_{0};
  bool opened_{false};
  bool initialized_{false};
};

}  // namespace binary_storage
}  // namespace esphome

#endif  // USE_BINARY_STORAGE_NVS || USE_ESP32_PREFERENCES_STORAGE
