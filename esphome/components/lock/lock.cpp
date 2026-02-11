#include "lock.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::lock {

static const char *const TAG = "lock";

// Lock state strings indexed by LockState enum (0-5): NONE(UNKNOWN), LOCKED, UNLOCKED, JAMMED, LOCKING, UNLOCKING
// Index 0 is UNKNOWN (for LOCK_STATE_NONE), also used as fallback for out-of-range
PROGMEM_STRING_TABLE(LockStateStrings, "UNKNOWN", "LOCKED", "UNLOCKED", "JAMMED", "LOCKING", "UNLOCKING");

const LogString *lock_state_to_string(LockState state) {
  return LockStateStrings::get_log_str(static_cast<uint8_t>(state), 0);
}

Lock::Lock() : state(LOCK_STATE_NONE) {}
LockCall Lock::make_call() { return LockCall(this); }

void Lock::set_state_(LockState state) {
  auto call = this->make_call();
  call.set_state(state);
  this->control(call);
}

void Lock::lock() { this->set_state_(LOCK_STATE_LOCKED); }
void Lock::unlock() { this->set_state_(LOCK_STATE_UNLOCKED); }
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
  ESP_LOGD(TAG, "'%s' >> %s", this->name_.c_str(), LOG_STR_ARG(lock_state_to_string(state)));
  this->state_callback_.call();
#if defined(USE_LOCK) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_lock_update(this);
#endif
}

void Lock::add_on_state_callback(std::function<void()> &&callback) { this->state_callback_.add(std::move(callback)); }

void LockCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  this->validate_();
  if (this->state_.has_value()) {
    ESP_LOGD(TAG, "  State: %s", LOG_STR_ARG(lock_state_to_string(*this->state_)));
  }
  this->parent_->control(*this);
}
void LockCall::validate_() {
  if (this->state_.has_value()) {
    auto state = *this->state_;
    if (!this->parent_->traits.supports_state(state)) {
      ESP_LOGW(TAG, "  State %s is not supported by this device!", LOG_STR_ARG(lock_state_to_string(*this->state_)));
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
LockCall &LockCall::set_state(const char *state) {
  if (ESPHOME_strcasecmp_P(state, ESPHOME_PSTR("LOCKED")) == 0) {
    this->set_state(LOCK_STATE_LOCKED);
  } else if (ESPHOME_strcasecmp_P(state, ESPHOME_PSTR("UNLOCKED")) == 0) {
    this->set_state(LOCK_STATE_UNLOCKED);
  } else if (ESPHOME_strcasecmp_P(state, ESPHOME_PSTR("JAMMED")) == 0) {
    this->set_state(LOCK_STATE_JAMMED);
  } else if (ESPHOME_strcasecmp_P(state, ESPHOME_PSTR("LOCKING")) == 0) {
    this->set_state(LOCK_STATE_LOCKING);
  } else if (ESPHOME_strcasecmp_P(state, ESPHOME_PSTR("UNLOCKING")) == 0) {
    this->set_state(LOCK_STATE_UNLOCKING);
  } else if (ESPHOME_strcasecmp_P(state, ESPHOME_PSTR("NONE")) == 0) {
    this->set_state(LOCK_STATE_NONE);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized state %s", this->parent_->get_name().c_str(), state);
  }
  return *this;
}
const optional<LockState> &LockCall::get_state() const { return this->state_; }

}  // namespace esphome::lock
