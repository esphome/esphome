#include "switch.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::switch_ {

ESPHOME_LOG_TAG(TAG, "switch");

Switch::Switch() : state(false) {}

void Switch::control(bool target_state) {
  if (target_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}
void Switch::turn_on() {
  ESP_LOGV(TAG, "'%s' Turning ON.", this->get_name().c_str());
  this->write_state(!this->inverted_);
}
void Switch::turn_off() {
  ESP_LOGV(TAG, "'%s' Turning OFF.", this->get_name().c_str());
  this->write_state(this->inverted_);
}
void Switch::toggle() {
  ESP_LOGV(TAG, "'%s' Toggling %s.", this->get_name().c_str(),
           this->state ? LOG_STR_LITERAL("OFF") : LOG_STR_LITERAL("ON"));
  this->write_state(this->inverted_ == this->state);
}
#ifdef USE_SWITCH_RESTORE_MODE_ON_RESET
SwitchRestoreMode Switch::effective_restore_mode_() {
  if (this->reset_overrides_ == nullptr)
    return this->restore_mode;
  const uint8_t cause = static_cast<uint8_t>(arch_get_reset_cause());
  // UNKNOWN has no YAML spelling, so it is never in the table.
  for (uint8_t i = 0; i < this->reset_override_count_; i++) {
    if (this->reset_overrides_[i * 2] == cause)
      return static_cast<SwitchRestoreMode>(this->reset_overrides_[i * 2 + 1]);
  }
  return this->restore_mode;
}
bool Switch::is_persistent_() {
  if (this->restore_mode & RESTORE_MODE_PERSISTENT_MASK)
    return true;
  // Scanned rather than cached: a cached flag would cost a byte on every switch.
  for (uint8_t i = 0; i < this->reset_override_count_; i++) {
    if (this->reset_overrides_[i * 2 + 1] & RESTORE_MODE_PERSISTENT_MASK)
      return true;
  }
  return false;
}
#endif  // USE_SWITCH_RESTORE_MODE_ON_RESET
optional<bool> Switch::get_initial_state() {
  if (!this->is_persistent_())
    return {};

  this->rtc_ = this->make_entity_preference<bool>();
  bool initial_state;
  if (!this->rtc_.load(&initial_state))
    return {};
  return initial_state;
}
optional<bool> Switch::get_initial_state_with_restore_mode() {
  // Before any early return: whether rtc_ is bound, and which slot sequential backends (esp8266, rp2) hand out,
  // must depend only on the configuration, never on this boot's cause.
  optional<bool> restored_state = this->get_initial_state();

  const SwitchRestoreMode mode = this->effective_restore_mode_();
  if (mode & RESTORE_MODE_DISABLED_MASK) {
    return {};
  }
  bool initial_state = mode & RESTORE_MODE_ON_MASK;  // default value *_OFF or *_ON
  // Required: a state can be stored on a boot whose effective mode is not persistent.
  if ((mode & RESTORE_MODE_PERSISTENT_MASK) && restored_state.has_value()) {  // For RESTORE_*
    // Invert value if any of the *_INVERTED_* modes
    initial_state = mode & RESTORE_MODE_INVERTED_MASK ? !restored_state.value() : restored_state.value();
  }
  return initial_state;
}
void Switch::publish_state(bool state) {
  if (!this->publish_dedup_.next(state))
    return;
  this->state = state != this->inverted_;
  this->set_has_state(true);

  if (this->is_persistent_())
    this->rtc_.save(&this->state);

  ESP_LOGV(TAG, "'%s' >> %s", this->name_.c_str(), ONOFF(this->state));
  this->state_callback_.call(this->state);
#if defined(USE_SWITCH) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_switch_update(this);
#endif
}
bool Switch::assumed_state() { return false; }

void log_switch(const char *tag, const char *prefix, const char *type, Switch *obj) {
  if (obj != nullptr) {
    // Prepare restore mode string
    const LogString *onoff = LOG_STR(""), *inverted = onoff, *restore;
    if (obj->restore_mode & RESTORE_MODE_DISABLED_MASK) {
      restore = LOG_STR("disabled");
    } else {
      onoff = obj->restore_mode & RESTORE_MODE_ON_MASK ? LOG_STR("ON") : LOG_STR("OFF");
      inverted = obj->restore_mode & RESTORE_MODE_INVERTED_MASK ? LOG_STR("inverted ") : LOG_STR("");
      restore = obj->restore_mode & RESTORE_MODE_PERSISTENT_MASK ? LOG_STR("restore defaults to") : LOG_STR("always");
    }

    // Build the base message with mandatory fields
    ESP_LOGCONFIG(tag,
                  "%s%s '%s'\n"
                  "%s  Restore Mode: %s%s %s",
                  prefix, type, obj->get_name().c_str(), prefix, LOG_STR_ARG(inverted), LOG_STR_ARG(restore),
                  LOG_STR_ARG(onoff));

#ifdef USE_SWITCH_RESTORE_MODE_ON_RESET
    if (obj->has_reset_overrides()) {
      ESP_LOGCONFIG(tag, "%s  Restore Mode is overridden for some reset causes", prefix);
    }
#endif

    // Add optional fields separately
    LOG_ENTITY_ICON(tag, prefix, *obj);
    if (obj->assumed_state()) {
      ESP_LOGCONFIG(tag, "%s  Assumed State: YES", prefix);
    }
    if (obj->is_inverted()) {
      ESP_LOGCONFIG(tag, "%s  Inverted: YES", prefix);
    }
    LOG_ENTITY_DEVICE_CLASS(tag, prefix, *obj);
  }
}

}  // namespace esphome::switch_
