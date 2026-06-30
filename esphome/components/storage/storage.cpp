#include "storage.h"
#include "esphome/core/log.h"

namespace esphome::storage {

static const char *const TAG = "storage";

StorageRegistry *global_storage_registry = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void StorageRegistry::register_storage(Storage *s) {
  if (s == nullptr)
    return;

  for (auto *existing : this->storages_) {
    if (existing == s)
      return;  // already registered
  }

  if (this->storages_.full()) {
    ESP_LOGE(TAG, "Registry full — increase device count");
    return;
  }
  this->storages_.push_back(s);

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

  // Null the slot of the matching entry, then compact by rebuilding.
  // FixedVector has no pop_back — we clear and re-push all non-null, non-removed entries.
  bool found = false;
  for (auto *entry : this->storages_) {
    if (entry == s) {
      found = true;
      break;
    }
  }
  if (!found)
    return;

  // Collect pointers to keep (all except s), clear, re-push
  // storages_ is sized exactly at setup — this is safe because we always remove one entry
  // before rebuilding, so we never exceed capacity.
  FixedVector<Storage *> tmp;
  tmp.init(this->storages_.size());
  for (auto *entry : this->storages_) {
    if (entry != s)
      tmp.push_back(entry);
  }
  this->storages_ = std::move(tmp);

  StorageInfo info{};
  if (s->get_info(&info) == StorageError::OK) {
    ESP_LOGI(TAG, "Storage unregistered: %s", info.name != nullptr ? info.name : "?");
  }

  this->on_unregistered_.call(s);
}

void StorageRegistry::for_each(void (*cb)(Storage *s, void *ctx), void *ctx) {
  for (auto *s : this->storages_) {
    cb(s, ctx);
  }
}

void StorageRegistry::for_each_filesystem(void (*cb)(FilesystemStorage *s, void *ctx), void *ctx) {
  for (auto *s : this->storages_) {
    if (s->get_storage_type() == StorageType::FILESYSTEM)
      cb(static_cast<FilesystemStorage *>(s), ctx);
  }
}

void StorageRegistry::for_each_raw(void (*cb)(RawStorage *s, void *ctx), void *ctx) {
  for (auto *s : this->storages_) {
    if (s->get_storage_type() == StorageType::RAW)
      cb(static_cast<RawStorage *>(s), ctx);
  }
}

void StorageRegistry::for_each_network(void (*cb)(NetworkStorage *s, void *ctx), void *ctx) {
  for (auto *s : this->storages_) {
    if (s->get_storage_type() == StorageType::NETWORK)
      cb(static_cast<NetworkStorage *>(s), ctx);
  }
}

}  // namespace esphome::storage
