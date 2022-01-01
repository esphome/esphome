#include "lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lock {

static const char *const TAG = "lock";

const LogString *lock_state_to_log_string(LockState mode) {
  switch (mode) {
    case LOCK_STATE_LOCKED:
      return LOG_STR("LOCKED");
    case LOCK_STATE_UNLOCKED:
      return LOG_STR("UNLOCKED");
    case LOCK_STATE_JAMMED:
      return LOG_STR("JAMMED");
    case LOCK_STATE_LOCKING:
      return LOG_STR("LOCKING");
    case LOCK_STATE_UNLOCKING:
      return LOG_STR("UNLOCKING");
    case LOCK_STATE_NONE:
    default:
      return LOG_STR("UNKNOWN");
  }
}

std::string lock_state_to_string(LockState mode) {
  switch (mode) {
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

void Lock::lock() {
  ESP_LOGD(TAG, "'%s' LOCKING.", this->get_name().c_str());
  this->write_state(LOCK_STATE_LOCKED);
}
void Lock::unlock() {
  ESP_LOGD(TAG, "'%s' UNLOCKING.", this->get_name().c_str());
  this->write_state(LOCK_STATE_UNLOCKED);
}
void Lock::open() {
  if (supports_open) {
    ESP_LOGD(TAG, "'%s' Opening.", this->get_name().c_str());
    this->open_latch();
  } else {
    ESP_LOGD(TAG, "'%s' Does not support Open.", this->get_name().c_str());
  }
}
void Lock::publish_state(LockState state) {
  if (!this->publish_dedup_.next(state))
    return;

  this->state = state;
  this->rtc_.save(&this->state);
  ESP_LOGD(TAG, "'%s': Sending state %s", this->name_.c_str(), lock_state_to_log_string(state));
  this->state_callback_.call();
}
bool Lock::assumed_state() { return false; }

void Lock::add_on_state_callback(std::function<void()> &&callback) { this->state_callback_.add(std::move(callback)); }
uint32_t Lock::hash_base() { return 3129890955UL; }

}  // namespace lock
}  // namespace esphome
