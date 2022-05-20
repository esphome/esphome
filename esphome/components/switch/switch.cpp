#include "switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace switch_ {

static const char *const TAG = "switch";

Switch::Switch(const std::string &name) : EntityBase(name), state(false) {}
Switch::Switch() : Switch("") {}

void Switch::write_state_on_(bool set_lock) {
  if(this->is_locked()) {
    ESP_LOGD(TAG, "'%s' can not turn ON because LOCKED.", this->get_name().c_str());
    return;
  }
  if(set_lock)
    this->lock();
  ESP_LOGD(TAG, "'%s' Turning ON.", this->get_name().c_str());
  this->write_state(!this->inverted_);
}
void Switch::write_state_off_(bool set_lock) {
  if(this->is_locked()) {
    ESP_LOGD(TAG, "'%s' can not turn OFF because LOCKED.", this->get_name().c_str());
    return;
  }
  if(set_lock)
    this->lock();
  ESP_LOGD(TAG, "'%s' Turning OFF.", this->get_name().c_str());
  this->write_state(this->inverted_);
}
void Switch::write_state_toggle_(bool set_lock) {
  if(this->is_locked()) {
    ESP_LOGD(TAG, "'%s' can not Toggle because LOCKED.", this->get_name().c_str());
    return;
  }
  if(set_lock)
    this->lock();
  ESP_LOGD(TAG, "'%s' Toggling %s.", this->get_name().c_str(), this->state ? "OFF" : "ON");
  this->write_state(this->inverted_ == this->state);
}

void Switch::lock() {
  ESP_LOGD(TAG, "'%s' Locked, can not be switched until unlocked", this->get_name().c_str());
  this->locked_ = true;
}
void Switch::unlock() {
  ESP_LOGD(TAG, "'%s' Unlocked, can now be switched", this->get_name().c_str());
  this->locked_ = false;
}

void Switch::turn_on()  { bool set_lock = false; this->write_state_on_(set_lock); }
void Switch::turn_off() { bool set_lock = false; this->write_state_off_(set_lock); }
void Switch::toggle()   { bool set_lock = false; this->write_state_toggle_(set_lock); }

void Switch::lock_turn_on()  { bool set_lock = true; this->write_state_on_(set_lock); }
void Switch::lock_turn_off() { bool set_lock = true; this->write_state_off_(set_lock); }
void Switch::lock_toggle()   { bool set_lock = true; this->write_state_toggle_(set_lock); }

bool Switch::is_locked() { return this->locked_; }
bool Switch::is_unlocked() { return ! this->locked_; }

optional<bool> Switch::get_initial_state() {
  this->rtc_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  bool initial_state;
  if (!this->rtc_.load(&initial_state))
    return {};
  return initial_state;
}
void Switch::publish_state(bool state) {
  if (!this->publish_dedup_.next(state))
    return;
  this->state = state != this->inverted_;

  this->rtc_.save(&this->state);
  ESP_LOGD(TAG, "'%s': Sending state %s", this->name_.c_str(), ONOFF(this->state));
  this->state_callback_.call(this->state);
}
bool Switch::assumed_state() { return false; }

void Switch::add_on_state_callback(std::function<void(bool)> &&callback) {
  this->state_callback_.add(std::move(callback));
}
void Switch::set_inverted(bool inverted) { this->inverted_ = inverted; }
uint32_t Switch::hash_base() { return 3129890955UL; }
bool Switch::is_inverted() const { return this->inverted_; }

std::string Switch::get_device_class() {
  if (this->device_class_.has_value())
    return *this->device_class_;
  return "";
}
void Switch::set_device_class(const std::string &device_class) { this->device_class_ = device_class; }

}  // namespace switch_
}  // namespace esphome
