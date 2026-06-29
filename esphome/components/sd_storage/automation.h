#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/defines.h"
#include "sd_storage_base.h"

#if defined(USE_SD_STORAGE_SDMMC)
#include "sd_storage.h"
#endif

#if defined(USE_SD_STORAGE_SPI)
#include "sd_storage_spi.h"
#endif

namespace esphome::sd_storage {

// Trigger — works with both SdMmc and SdSpi via SdStorageBase
template<typename T> class CardMountedTrigger : public Trigger<const char *> {
 public:
  explicit CardMountedTrigger(T *parent) {
    parent->add_on_mounted_callback([this](const char *mount_path) { this->trigger(mount_path); });
  }
};

// Actions
template<typename T, typename... Ts> class MountCardAction : public Action<Ts...> {
 public:
  explicit MountCardAction(T *parent) : parent_(parent) {}

  void play(Ts... x) override {
    if (this->parent_->mount() == StorageError::OK) {
      ESP_LOGI("sd_storage", "Card mounted via automation");
    } else {
      ESP_LOGE("sd_storage", "Failed to mount card via automation");
    }
  }

 protected:
  T *parent_;
};

template<typename T, typename... Ts> class UnmountCardAction : public Action<Ts...> {
 public:
  explicit UnmountCardAction(T *parent) : parent_(parent) {}

  void play(Ts... x) override {
    this->parent_->unmount();
    ESP_LOGI("sd_storage", "Card unmounted via automation");
  }

 protected:
  T *parent_;
};

template<typename T, typename... Ts> class ListFilesAction : public Action<Ts...> {
 public:
  explicit ListFilesAction(T *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(const char *, path)

  void play(Ts... x) override {
    const char *path = this->path_.value(x...);
    if (path == nullptr || path[0] == '\0')
      path = this->parent_->get_mount_path();

    auto cb = [](const storage::FileStat *entry, void *ctx) {
      if (entry->is_dir) {
        ESP_LOGI("sd_storage", "  [DIR]  %s", entry->name);
      } else {
        ESP_LOGI("sd_storage", "  [FILE] %s (%zu bytes)", entry->name, entry->size);
      }
    };
    ESP_LOGI("sd_storage", "Listing files in: %s", path);
    this->parent_->list_dir(path, cb, nullptr);
  }

 protected:
  T *parent_;
};

// Condition
template<typename T, typename... Ts> class CardMountedCondition : public Condition<Ts...> {
 public:
  explicit CardMountedCondition(T *parent) : parent_(parent) {}

  bool check(Ts... x) override { return this->parent_->is_mounted(); }

 protected:
  T *parent_;
};

}  // namespace esphome::sd_storage
