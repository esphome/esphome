#pragma once

#ifdef USE_ESP_IDF

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

// Triggers — work with both SdMmc and SdSpi via SdStorageBase
class CardMountedTrigger : public Trigger<const char *> {
 public:
  explicit CardMountedTrigger(SdStorageBase *parent) {
    parent->add_on_mounted_callback([this](const char *mount_path) { this->trigger(mount_path); });
  }
};

class CardRemovedTrigger : public Trigger<> {
 public:
  explicit CardRemovedTrigger(SdStorageBase *parent) {
    parent->add_on_removed_callback([this]() { this->trigger(); });
  }
};

class CardInsertedTrigger : public Trigger<> {
 public:
  explicit CardInsertedTrigger(SdStorageBase *parent) {
    parent->add_on_inserted_callback([this]() { this->trigger(); });
  }
};

// Actions
template<typename... Ts> class MountCardAction : public Action<Ts...> {
 public:
  explicit MountCardAction(SdStorageBase *parent) : parent_(parent) {}

  void play(Ts... x) override {
    bool ok = this->parent_->mount() == storage::StorageError::OK;
    this->parent_->log_mount_result_(ok);
  }

 protected:
  SdStorageBase *parent_;
};

template<typename... Ts> class UnmountCardAction : public Action<Ts...> {
 public:
  explicit UnmountCardAction(SdStorageBase *parent) : parent_(parent) {}

  void play(Ts... x) override {
    this->parent_->unmount();
    this->parent_->log_unmount_();
  }

 protected:
  SdStorageBase *parent_;
};

template<typename... Ts> class ListFilesAction : public Action<Ts...> {
 public:
  explicit ListFilesAction(SdStorageBase *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(const char *, path)

  void play(Ts... x) override {
    const char *path = this->path_.value(x...);
    if (path == nullptr || path[0] == '\0')
      path = this->parent_->get_mount_path();

    this->parent_->log_list_dir_start_(path);
    this->parent_->list_dir(path, &SdStorageBase::log_list_dir_entry, nullptr);
  }

 protected:
  SdStorageBase *parent_;
};

// Condition
template<typename... Ts> class CardMountedCondition : public Condition<Ts...> {
 public:
  explicit CardMountedCondition(SdStorageBase *parent) : parent_(parent) {}

  bool check(Ts... x) override { return this->parent_->is_mounted(); }

 protected:
  SdStorageBase *parent_;
};

}  // namespace esphome::sd_storage

#endif  // USE_ESP_IDF
