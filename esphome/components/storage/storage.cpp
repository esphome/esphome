#include "storage.h"
#include "esphome/core/log.h"

namespace esphome {
namespace storage {

static const char *const TAG = "storage";

StorageRegistry *global_storage_registry = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void StorageRegistry::register_storage(Storage *s) {
  if (s == nullptr)
    return;

  for (size_t i = 0; i < this->storages_.size(); i++) {
    if (this->storages_[i] == s)
      return;  // already registered
  }

  if (!this->storages_.push_back(s)) {
    ESP_LOGE(TAG, "Registry full — increase device count");
    return;
  }

  StorageInfo info{};
  if (s->get_info(&info) == StorageError::OK) {
    ESP_LOGI(TAG, "Storage registered: %s (%s)", info.name != nullptr ? info.name : "?",
             info.id != nullptr ? info.id : "?");
  }

  this->on_registered_.call(s);
}

void StorageRegistry::unregister_storage(Storage *s) {
  if (s == nullptr)
    return;

  for (size_t i = 0; i < this->storages_.size(); i++) {
    if (this->storages_[i] == s) {
      // Shift remaining entries down to fill the gap
      for (size_t j = i + 1; j < this->storages_.size(); j++) {
        this->storages_[j - 1] = this->storages_[j];
      }
      this->storages_.pop_back();

      StorageInfo info{};
      if (s->get_info(&info) == StorageError::OK) {
        ESP_LOGI(TAG, "Storage unregistered: %s", info.name != nullptr ? info.name : "?");
      }

      this->on_unregistered_.call(s);
      return;
    }
  }
}

void StorageRegistry::for_each(void (*cb)(Storage *s, void *ctx), void *ctx) {
  for (size_t i = 0; i < this->storages_.size(); i++) {
    cb(this->storages_[i], ctx);
  }
}

void StorageRegistry::for_each_filesystem(void (*cb)(FilesystemStorage *s, void *ctx), void *ctx) {
  for (size_t i = 0; i < this->storages_.size(); i++) {
    if (auto *fs = dynamic_cast<FilesystemStorage *>(this->storages_[i]))
      cb(fs, ctx);
  }
}

void StorageRegistry::for_each_raw(void (*cb)(RawStorage *s, void *ctx), void *ctx) {
  for (size_t i = 0; i < this->storages_.size(); i++) {
    if (auto *rs = dynamic_cast<RawStorage *>(this->storages_[i]))
      cb(rs, ctx);
  }
}

void StorageRegistry::for_each_network(void (*cb)(NetworkStorage *s, void *ctx), void *ctx) {
  for (size_t i = 0; i < this->storages_.size(); i++) {
    if (auto *ns = dynamic_cast<NetworkStorage *>(this->storages_[i]))
      cb(ns, ctx);
  }
}

}  // namespace storage
}  // namespace esphome
