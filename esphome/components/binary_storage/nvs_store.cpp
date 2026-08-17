#include "nvs_store.h"

#if defined(USE_BINARY_STORAGE_NVS) || defined(USE_ESP32_PREFERENCES_STORAGE)

#include "esphome/core/log.h"

#include <nvs_flash.h>
#include <cstdio>
#include <cstdlib>

namespace esphome {
namespace binary_storage {

static const char *const TAG = "binary_storage.nvs";

// uint32 key -> NVS string key: decimal, matching esp32 preferences (uint32_to_str) so a future
// preferences adoption can share this namespace. An NVS key is at most 15 chars; a uint32 is <= 10.
static void key_to_str(uint32_t key, char *out, size_t len) { snprintf(out, len, "%u", (unsigned int) key); }

// A null partition_label means the system default NVS partition (nvs_flash_init / nvs_open); a label
// selects a dedicated one (..._partition variants). This lets one class serve both a YAML-configured
// dedicated store and, from C++, the system "nvs" partition (e.g. for preferences).
static esp_err_t nvs_init_for(const char *label) {
  return label != nullptr ? nvs_flash_init_partition(label) : nvs_flash_init();
}
static esp_err_t nvs_erase_for(const char *label) {
  return label != nullptr ? nvs_flash_erase_partition(label) : nvs_flash_erase();
}
static const char *label_str(const char *label) { return label != nullptr ? label : "nvs"; }

void NVSStore::setup() {
  if (this->ensure_initialized() != storage::StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "NVS init failed for partition '%s'", this->partition_label_);
    this->mark_failed();
    return;
  }
  if (storage::global_storage_registry != nullptr) {
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::STORAGE_ERROR_OK) {
      ESP_LOGE(TAG, "Storage registration failed");
      this->mark_failed();
    }
  }
}

storage::StorageError NVSStore::ensure_initialized() {
  if (this->initialized_)
    return storage::StorageError::STORAGE_ERROR_OK;
  esp_err_t err = nvs_init_for(this->partition_label_);
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // Empty or version-bumped medium: erase and re-init. This is the detect-and-format path an
    // external device hits on first boot; an internal partition already laid out is the fast case.
    ESP_LOGW(TAG, "NVS partition '%s' empty/outdated, formatting", label_str(this->partition_label_));
    err = nvs_erase_for(this->partition_label_);
    if (err == ESP_OK)
      err = nvs_init_for(this->partition_label_);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs init('%s'): %s", label_str(this->partition_label_), esp_err_to_name(err));
    return storage::StorageError::STORAGE_ERROR_NOT_READY;
  }
  this->initialized_ = true;
  return storage::StorageError::STORAGE_ERROR_OK;
}

bool NVSStore::open_() {
  if (this->opened_)
    return true;
  if (!this->initialized_ && this->ensure_initialized() != storage::StorageError::STORAGE_ERROR_OK)
    return false;
  esp_err_t err = this->partition_label_ != nullptr
                      ? nvs_open_from_partition(this->partition_label_, this->namespace_, NVS_READWRITE, &this->handle_)
                      : nvs_open(this->namespace_, NVS_READWRITE, &this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open('%s','%s'): %s", label_str(this->partition_label_), this->namespace_, esp_err_to_name(err));
    return false;
  }
  this->opened_ = true;
  return true;
}

storage::StorageError NVSStore::get(uint32_t key, uint8_t *buf, size_t len, size_t *got) {
  *got = 0;
  if (!this->open_())
    return storage::StorageError::STORAGE_ERROR_NOT_READY;
  char k[16];
  key_to_str(key, k, sizeof(k));
  size_t need = 0;
  esp_err_t err = nvs_get_blob(this->handle_, k, nullptr, &need);
  if (err == ESP_ERR_NVS_NOT_FOUND)
    return storage::StorageError::STORAGE_ERROR_NOT_FOUND;
  if (err != ESP_OK)
    return storage::StorageError::STORAGE_ERROR_READ_ERROR;
  if (need > len)
    return storage::StorageError::STORAGE_ERROR_INVALID_ARGS;  // caller should query get_size() first
  err = nvs_get_blob(this->handle_, k, buf, &need);
  if (err != ESP_OK)
    return storage::StorageError::STORAGE_ERROR_READ_ERROR;
  *got = need;
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError NVSStore::set(uint32_t key, const uint8_t *data, size_t len) {
  if (!this->open_())
    return storage::StorageError::STORAGE_ERROR_NOT_READY;
  char k[16];
  key_to_str(key, k, sizeof(k));
  esp_err_t err = nvs_set_blob(this->handle_, k, data, len);
  if (err == ESP_OK)
    err = nvs_commit(this->handle_);
  return err == ESP_OK ? storage::StorageError::STORAGE_ERROR_OK : storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
}

storage::StorageError NVSStore::erase(uint32_t key) {
  if (!this->open_())
    return storage::StorageError::STORAGE_ERROR_NOT_READY;
  char k[16];
  key_to_str(key, k, sizeof(k));
  esp_err_t err = nvs_erase_key(this->handle_, k);
  if (err == ESP_ERR_NVS_NOT_FOUND)
    return storage::StorageError::STORAGE_ERROR_OK;  // idempotent: erasing an absent key is success
  if (err == ESP_OK)
    err = nvs_commit(this->handle_);
  return err == ESP_OK ? storage::StorageError::STORAGE_ERROR_OK : storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
}

bool NVSStore::has(uint32_t key) {
  if (!this->open_())
    return false;
  char k[16];
  key_to_str(key, k, sizeof(k));
  size_t need = 0;
  return nvs_get_blob(this->handle_, k, nullptr, &need) == ESP_OK;
}

storage::StorageError NVSStore::get_size(uint32_t key, size_t *out) {
  *out = 0;
  if (!this->open_())
    return storage::StorageError::STORAGE_ERROR_NOT_READY;
  char k[16];
  key_to_str(key, k, sizeof(k));
  size_t need = 0;
  esp_err_t err = nvs_get_blob(this->handle_, k, nullptr, &need);
  if (err == ESP_ERR_NVS_NOT_FOUND)
    return storage::StorageError::STORAGE_ERROR_NOT_FOUND;
  if (err != ESP_OK)
    return storage::StorageError::STORAGE_ERROR_READ_ERROR;
  *out = need;
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError NVSStore::list_keys(bool (*callback)(uint32_t key, size_t size, void *ctx), void *ctx) {
  if (!this->open_())
    return storage::StorageError::STORAGE_ERROR_NOT_READY;
  // A null partition_label means the system default partition; nvs_entry_find needs its real name.
  const char *part = this->partition_label_ != nullptr ? this->partition_label_ : NVS_DEFAULT_PART_NAME;
  nvs_iterator_t it = nullptr;
  esp_err_t err = nvs_entry_find(part, this->namespace_, NVS_TYPE_BLOB, &it);
  while (err == ESP_OK && it != nullptr) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    // Keys are decimal (key_to_str's "%u"); skip anything that does not round-trip to a uint32.
    char *end = nullptr;
    unsigned long parsed = strtoul(info.key, &end, 10);
    if (end != nullptr && *end == '\0') {
      size_t need = 0;
      if (nvs_get_blob(this->handle_, info.key, nullptr, &need) == ESP_OK &&
          !callback(static_cast<uint32_t>(parsed), need, ctx)) {
        nvs_release_iterator(it);
        return storage::StorageError::STORAGE_ERROR_OK;  // callback asked to stop
      }
    }
    err = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError NVSStore::format() {
  // Destructive: drop the handle, erase the whole partition, re-init empty.
  if (this->opened_) {
    nvs_close(this->handle_);
    this->opened_ = false;
  }
  this->initialized_ = false;
  esp_err_t err = nvs_erase_for(this->partition_label_);
  if (err == ESP_OK)
    err = nvs_init_for(this->partition_label_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "format('%s'): %s", label_str(this->partition_label_), esp_err_to_name(err));
    return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
  }
  this->initialized_ = true;
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError NVSStore::get_info(storage::StorageInfo *info) {
  info->id = this->storage_id_;
  info->name = this->storage_name_ != nullptr ? this->storage_name_ : label_str(this->partition_label_);
  info->kind = "nvs";
  info->total_bytes = 0;  // NVS does not expose a simple byte total
  info->free_bytes = 0;
  info->block_size = 0;
  info->is_mounted = this->initialized_;
  info->is_removable = false;
  info->is_read_only = false;
  return storage::StorageError::STORAGE_ERROR_OK;
}

void NVSStore::dump_config() {
  ESP_LOGCONFIG(TAG, "NVS key-value store:");
  ESP_LOGCONFIG(TAG, "  Partition: %s", label_str(this->partition_label_));
  ESP_LOGCONFIG(TAG, "  Namespace: %s", this->namespace_);
}

}  // namespace binary_storage
}  // namespace esphome

#endif  // USE_BINARY_STORAGE_NVS || USE_ESP32_PREFERENCES_STORAGE
