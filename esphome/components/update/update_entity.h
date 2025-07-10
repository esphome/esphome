#pragma once

#include <memory>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"

namespace esphome {
namespace update {

struct UpdateInfo {
  std::string latest_version;
  std::string current_version;
  std::string title;
  std::string summary;
  std::string release_url;
  std::string firmware_url;
  std::string md5;
  bool has_progress{false};
  float progress;
};

enum UpdateState : uint8_t {
  UPDATE_STATE_UNKNOWN,
  UPDATE_STATE_NO_UPDATE,
  UPDATE_STATE_AVAILABLE,
  UPDATE_STATE_INSTALLING,
};

class UpdateEntity : public EntityBase, public EntityBase_DeviceClass {
 public:
  void publish_state();

  void perform() { this->perform(false); }
  virtual void perform(bool force) = 0;
  virtual void check() = 0;

  const UpdateInfo &update_info = update_info_;
  const UpdateState &state = state_;

  void add_on_state_callback(std::function<void()> &&callback) { this->state_callback_.add(std::move(callback)); }
  Trigger<const UpdateInfo &> *get_update_available_trigger() {
    if (!update_available_trigger_) {
      update_available_trigger_ = std::make_unique<Trigger<const UpdateInfo &>>();
    }
    return update_available_trigger_.get();
  }

 protected:
  UpdateState state_{UPDATE_STATE_UNKNOWN};
  UpdateInfo update_info_;

  CallbackManager<void()> state_callback_{};
  std::unique_ptr<Trigger<const UpdateInfo &>> update_available_trigger_{nullptr};
};

}  // namespace update
}  // namespace esphome
