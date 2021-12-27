#include "lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lock_ {

static const char *const TAG = "lock";

Lock::Lock(const std::string &name) : EntityBase(name), state(false) {}
Lock::Lock() : Lock("") {}

void Lock::lock() {
  ESP_LOGD(TAG, "'%s' LOCKING.", this->get_name().c_str());
  this->write_state(!this->inverted_);
}
void Lock::unlock() {
  ESP_LOGD(TAG, "'%s' UNLOCKING.", this->get_name().c_str());
  this->write_state(this->inverted_);
}
void Lock::toggle() {
  ESP_LOGD(TAG, "'%s' Toggling %s.", this->get_name().c_str(), this->state ? "UNLOCKED" : "LOCKED");
  this->write_state(this->inverted_ == this->state);
}
void Lock::open() {
  ESP_LOGD(TAG, "'%s' Opening.", this->get_name().c_str());
  this->open_latch()
}
optional<bool> Lock::get_initial_state() {
  this->rtc_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  bool initial_state;
  if (!this->rtc_.load(&initial_state))
    return {};
  return initial_state;
}
void Lock::publish_state(bool state) {
  if (!this->publish_dedup_.next(state))
    return;
  this->state = state != this->inverted_;

  this->rtc_.save(&this->state);
  ESP_LOGD(TAG, "'%s': Sending state %s", this->name_.c_str(), LOCKUNLOCK(state));
  this->state_callback_.call(this->state);
}
bool Lock::assumed_state() { return false; }

void Lock::add_on_state_callback(std::function<void(bool)> &&callback) {
  this->state_callback_.add(std::move(callback));
}
void Lock::set_inverted(bool inverted) { this->inverted_ = inverted; }
uint32_t Lock::hash_base() { return 3129890955UL; }
bool Lock::is_inverted() const { return this->inverted_; }

}  // namespace lock_
}  // namespace esphome
