#pragma once

#include "esphome/core/automation.h"
#include "sd_mmc_card.h"

namespace esphome {
namespace sd_mmc_card {

// Triggers
class CardMountedTrigger : public Trigger<std::string> {
 public:
  explicit CardMountedTrigger(SdMmc *parent) {
    parent->add_mount_ready_callback([this](const std::string &mount_path) { this->trigger(mount_path); });
  }
};

// Actions
template<typename... Ts> class MountCardAction : public Action<Ts...> {
 public:
  explicit MountCardAction(SdMmc *sd_mmc) : sd_mmc_(sd_mmc) {}

  void play(Ts... x) override {
    if (this->sd_mmc_->mount_card()) {
      ESP_LOGI("sd_mmc_card", "Card mounted successfully via automation");
    } else {
      ESP_LOGE("sd_mmc_card", "Failed to mount card via automation");
    }
  }

 protected:
  SdMmc *sd_mmc_;
};

template<typename... Ts> class UnmountCardAction : public Action<Ts...> {
 public:
  explicit UnmountCardAction(SdMmc *sd_mmc) : sd_mmc_(sd_mmc) {}

  void play(Ts... x) override {
    this->sd_mmc_->unmount_card();
    ESP_LOGI("sd_mmc_card", "Card unmounted via automation");
  }

 protected:
  SdMmc *sd_mmc_;
};

template<typename... Ts> class ListFilesAction : public Action<Ts...> {
 public:
  explicit ListFilesAction(SdMmc *sd_mmc) : sd_mmc_(sd_mmc) {}

  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) override {
    auto path = this->path_.value(x...);
    if (path.empty()) {
      path = this->sd_mmc_->get_mount_path();
    }

    ESP_LOGI("sd_mmc_card", "Listing files in: %s", path.c_str());
    auto files = this->sd_mmc_->list_directory(path);
    for (const auto &file : files) {
      if (file.is_directory) {
        ESP_LOGI("sd_mmc_card", "  [DIR]  %s", file.path.c_str());
      } else {
        ESP_LOGI("sd_mmc_card", "  [FILE] %s (%u bytes)", file.path.c_str(), file.size);
      }
    }
    ESP_LOGI("sd_mmc_card", "Total: %zu items", files.size());
  }

 protected:
  SdMmc *sd_mmc_;
};

// Conditions
template<typename... Ts> class CardMountedCondition : public Condition<Ts...> {
 public:
  explicit CardMountedCondition(SdMmc *sd_mmc) : sd_mmc_(sd_mmc) {}

  bool check(Ts... x) override { return this->sd_mmc_->is_mounted(); }

 protected:
  SdMmc *sd_mmc_;
};

}  // namespace sd_mmc_card
}  // namespace esphome
