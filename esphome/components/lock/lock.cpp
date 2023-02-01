#include "lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lock {

static const char *const TAG = "lock";

const char *lock_state_to_string(LockState state) {
  switch (state) {
    case LOCK_STATE_LOCKED:
      return "LOCKED";
    case LOCK_STATE_UNLOCKED:
      return "UNLOCKED";
    case LOCK_STATE_JAMMED:
      return "JAMMED";
    case LOCK_STATE_LOCKING:
      return "LOCKING";
    case LOCK_STATE_UNLOCKING:
      return "UNLOCKING";
    case LOCK_STATE_NONE:
    default:
      return "UNKNOWN";
  }
}

Lock::Lock(const std::string &name) : EntityBase(name), state(LOCK_STATE_NONE) {}
Lock::Lock() : Lock("") {}
LockCall Lock::make_call() { return LockCall(this); }

void Lock::lock() {
  auto call = this->make_call();
  call.set_state(LOCK_STATE_LOCKED);
  this->control(call);
}
void Lock::unlock() {
  auto call = this->make_call();
  call.set_state(LOCK_STATE_UNLOCKED);
  this->control(call);
}
void Lock::open() {
  if (traits.get_supports_open()) {
    ESP_LOGD(TAG, "'%s' Opening.", this->get_name().c_str());
    this->open_latch();
  } else {
    ESP_LOGW(TAG, "'%s' Does not support Open.", this->get_name().c_str());
  }
}
void Lock::publish_state(LockState state) {
  if (!this->publish_dedup_.next(state))
    return;

  this->state = state;
  this->rtc_.save(&this->state);
  ESP_LOGD(TAG, "'%s': Sending state %s", this->name_.c_str(), lock_state_to_string(state));
  this->state_callback_.call();
}

void Lock::add_on_state_callback(std::function<void()> &&callback) { this->state_callback_.add(std::move(callback)); }

void LockCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  this->validate_();
  if (this->state_.has_value()) {
    const char *state_s = lock_state_to_string(*this->state_);
    ESP_LOGD(TAG, "  State: %s", state_s);
  }
  this->parent_->control(*this);
}
void LockCall::validate_() {
  if (this->state_.has_value()) {
    auto state = *this->state_;
    if (!this->parent_->traits.supports_state(state)) {
      ESP_LOGW(TAG, "  State %s is not supported by this device!", lock_state_to_string(*this->state_));
      this->state_.reset();
    }
  }
}
LockCall &LockCall::set_state(LockState state) {
  this->state_ = state;
  return *this;
}
LockCall &LockCall::set_state(optional<LockState> state) {
  this->state_ = state;
  return *this;
}
LockCall &LockCall::set_state(const std::string &state) {
  if (str_equals_case_insensitive(state, "LOCKED")) {
    this->set_state(LOCK_STATE_LOCKED);
  } else if (str_equals_case_insensitive(state, "UNLOCKED")) {
    this->set_state(LOCK_STATE_UNLOCKED);
  } else if (str_equals_case_insensitive(state, "JAMMED")) {
    this->set_state(LOCK_STATE_JAMMED);
  } else if (str_equals_case_insensitive(state, "LOCKING")) {
    this->set_state(LOCK_STATE_LOCKING);
  } else if (str_equals_case_insensitive(state, "UNLOCKING")) {
    this->set_state(LOCK_STATE_UNLOCKING);
  } else if (str_equals_case_insensitive(state, "NONE")) {
    this->set_state(LOCK_STATE_NONE);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized state %s", this->parent_->get_name().c_str(), state.c_str());
  }
  return *this;
}
const optional<LockState> &LockCall::get_state() const { return this->state_; }

}  // namespace lock
}  // namespace esphome
