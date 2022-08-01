#include "switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace switch_ {

static const char *const TAG = "switch";

Switch::Switch(const std::string &name) : EntityBase(name), state(false) {}
Switch::Switch() : Switch("") {}

void Switch::turn_on() {
  ESP_LOGD(TAG, "'%s' Turning ON.", this->get_name().c_str());
  this->write_state(!this->inverted_);
}
void Switch::turn_off() {
  ESP_LOGD(TAG, "'%s' Turning OFF.", this->get_name().c_str());
  this->write_state(this->inverted_);
}
void Switch::toggle() {
  ESP_LOGD(TAG, "'%s' Toggling %s.", this->get_name().c_str(), this->state ? "OFF" : "ON");
  this->write_state(this->inverted_ == this->state);
}
optional<bool> Switch::get_initial_state() {
  if (!(restore_mode & RESTORE_MODE_PERSISTENT_MASK))
    return {};

  this->rtc_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  bool initial_state;
  if (!this->rtc_.load(&initial_state))
    return {};
  return initial_state;
}
bool Switch::get_initial_state_with_restore_mode() {
  bool initial_state = restore_mode & RESTORE_MODE_ON_MASK;
  if (restore_mode & RESTORE_MODE_INVERTED_MASK)
    initial_state = !initial_state;
  if (restore_mode & RESTORE_MODE_PERSISTENT_MASK) {
    initial_state = this->get_initial_state().value_or(initial_state);
  }

  return initial_state;
}
void Switch::publish_state(bool state) {
  if (!this->publish_dedup_.next(state))
    return;
  this->state = state != this->inverted_;

  if (restore_mode & RESTORE_MODE_PERSISTENT_MASK)
    this->rtc_.save(&this->state);

  ESP_LOGD(TAG, "'%s': Sending state %s", this->name_.c_str(), ONOFF(this->state));
  this->state_callback_.call(this->state);
}
bool Switch::assumed_state() { return false; }

void Switch::add_on_state_callback(std::function<void(bool)> &&callback) {
  this->state_callback_.add(std::move(callback));
}
void Switch::set_inverted(bool inverted) { this->inverted_ = inverted; }
bool Switch::is_inverted() const { return this->inverted_; }

std::string Switch::get_device_class() {
  if (this->device_class_.has_value())
    return *this->device_class_;
  return "";
}
void Switch::set_device_class(const std::string &device_class) { this->device_class_ = device_class; }

void LOG_SWITCH_(const char *TAG, const char *prefix, const char *type, Switch *obj) {
  if (obj != nullptr) {
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), obj->get_name().c_str());
    if (!obj->get_icon().empty()) {
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, obj->get_icon().c_str());
    }
    if (obj->assumed_state()) {
      ESP_LOGCONFIG(TAG, "%s  Assumed State: YES", prefix);
    }
    if (obj->is_inverted()) {
      ESP_LOGCONFIG(TAG, "%s  Inverted: YES", prefix);
    }
    if (!obj->get_device_class().empty()) {
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, obj->get_device_class().c_str());
    }
  }
  const LogString *onoff = obj->restore_mode & RESTORE_MODE_ON_MASK ? LOG_STR("ON") : LOG_STR("OFF");
  const LogString *inverted = obj->restore_mode & RESTORE_MODE_INVERTED_MASK ? LOG_STR("inverted ") : LOG_STR("");
  const LogString *restore =
      obj->restore_mode & RESTORE_MODE_PERSISTENT_MASK ? LOG_STR("restore defaults to") : LOG_STR("always");

  ESP_LOGCONFIG(TAG, "%s  Restore Mode: %s%s %s", prefix, LOG_STR_ARG(inverted), LOG_STR_ARG(restore),
                LOG_STR_ARG(onoff));
}

}  // namespace switch_
}  // namespace esphome
