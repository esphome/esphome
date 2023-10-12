#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

optional<float> Number::get_initial_state() {
  if (!(restore_mode & RESTORE_MODE_PERSISTENT_MASK))
    return {};

  this->rtc_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  float initial_state;
  if (!this->rtc_.load(&initial_state))
    return {};
  return initial_state;
}

optional<float> Number::get_initial_state_with_restore_mode() {
  if (restore_mode & RESTORE_MODE_DISABLED_MASK) {
    return {};
  }
  float initial_state = this->restore_default_state;
  if (restore_mode & RESTORE_MODE_PERSISTENT_MASK) {
    optional<float> restored_state = this->get_initial_state();
    if (restored_state.has_value()) {
      initial_state = restored_state.value();
    }
  }
  return initial_state;
}

void Number::publish_state(float state) {
  this->has_state_ = true;
  this->state = state;

  if (restore_mode & RESTORE_MODE_PERSISTENT_MASK)
    this->rtc_.save(&this->state);

  ESP_LOGD(TAG, "'%s': Sending state %f", this->get_name().c_str(), state);
  this->state_callback_.call(state);
}

void Number::add_on_state_callback(std::function<void(float)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

}  // namespace number
}  // namespace esphome
