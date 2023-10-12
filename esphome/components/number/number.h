#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "number_call.h"
#include "number_traits.h"

namespace esphome {
namespace number {

#define LOG_NUMBER(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
    if (!(obj)->traits.get_unit_of_measurement().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Unit of Measurement: '%s'", prefix, (obj)->traits.get_unit_of_measurement().c_str()); \
    } \
    if (!(obj)->traits.get_device_class().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, (obj)->traits.get_device_class().c_str()); \
    }                                 \
    const LogString *restore; \
    if (obj->restore_mode & number::RESTORE_MODE_DISABLED_MASK) { \
      restore = LOG_STR("disabled"); \
    } else { \
      restore = obj->restore_mode & number::RESTORE_MODE_PERSISTENT_MASK ? LOG_STR("restore defaults to") : LOG_STR("INVALID");\
    } \
    ESP_LOGCONFIG(TAG, "%s  Restore Mode: %s %.5f", prefix, LOG_STR_ARG(restore), obj->restore_default_state);\
  }

#define SUB_NUMBER(name) \
 protected: \
  number::Number *name##_number_{nullptr}; \
\
 public: \
  void set_##name##_number(number::Number *number) { this->name##_number_ = number; }

// We are using the same bits as switch restore modes just to be nice and
// consistent.
// bit0: n/a. bit1: persistent. bit2: n/a. bit3: disabled
const int RESTORE_MODE_PERSISTENT_MASK = 0x02;
const int RESTORE_MODE_DISABLED_MASK = 0x08;

enum NumberRestoreMode {
  NUMBER_RESTORE_ENABLED = RESTORE_MODE_PERSISTENT_MASK,
  NUMBER_RESTORE_DISABLED = RESTORE_MODE_DISABLED_MASK,
};

class Number;

/** Base-class for all numbers.
 *
 * A number can use publish_state to send out a new value.
 */
class Number : public EntityBase {
 public:
  float state;

  void publish_state(float state);

  NumberCall make_call() { return NumberCall(this); }

  void add_on_state_callback(std::function<void(float)> &&callback);

  NumberTraits traits;

  /// Return whether this number has gotten a full state yet.
  bool has_state() const { return has_state_; }

  /// Indicates whether or not state is to be retrieved from flash and how
  NumberRestoreMode restore_mode{NUMBER_RESTORE_DISABLED};

  /// Default state to set when restore mode is enabled, but there is no saved value in flash memory.
  float restore_default_state;

  /** Returns the initial state of the switch, as persisted previously,
    or empty if never persisted.
   */
  optional<float> get_initial_state();

  /** Returns the initial state of the switch, after applying restore mode rules.
   * If restore mode is disabled, this function will return an optional with no value
   * (.has_value() is false), leaving it up to the component to decide the state.
   * For example, the component could read the state from hardware and determine the current
   * state.
   */
  optional<float> get_initial_state_with_restore_mode();

  void set_restore_mode(NumberRestoreMode restore_mode) { this->restore_mode = restore_mode; }
  void set_restore_default_state(float restore_default_state) { this->restore_default_state = restore_default_state; }

 protected:
  friend class NumberCall;

  /** Set the value of the number, this is a virtual method that each number integration must implement.
   *
   * This method is called by the NumberCall.
   *
   * @param value The value as validated by the NumberCall.
   */
  virtual void control(float value) = 0;

  CallbackManager<void(float)> state_callback_;
  bool has_state_{false};
  ESPPreferenceObject rtc_;
};

}  // namespace number
}  // namespace esphome
