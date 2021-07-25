#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace number {

#define LOG_NUMBER(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, type, (obj)->get_name().c_str()); \
    if (!(obj)->traits.get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->traits.get_icon().c_str()); \
    } \
  }

class Number;

class NumberCall {
 public:
  explicit NumberCall(Number *parent) : parent_(parent) {}
  void perform();

  NumberCall &set_value(float value) {
    value_ = value;
    return *this;
  }
  const optional<float> &get_value() const { return value_; }

 protected:
  Number *const parent_;
  optional<float> value_;
};

class NumberTraits {
 public:
  void set_min_value(float min_value) { min_value_ = min_value; }
  float get_min_value() const { return min_value_; }
  void set_max_value(float max_value) { max_value_ = max_value; }
  float get_max_value() const { return max_value_; }
  void set_step(float step) { step_ = step; }
  float get_step() const { return step_; }
  void set_icon(std::string icon) { icon_ = std::move(icon); }
  const std::string &get_icon() const { return icon_; }

 protected:
  float min_value_ = NAN;
  float max_value_ = NAN;
  float step_ = NAN;
  std::string icon_;
};

/** Base-class for all numbers.
 *
 * A number can use publish_state to send out a new value.
 */
class Number : public Nameable {
 public:
  float state;

  void publish_state(float state);

  NumberCall make_call() { return NumberCall(this); }
  void set(float value) { make_call().set_value(value).perform(); }

  void add_on_state_callback(std::function<void(float)> &&callback);

  NumberTraits traits;

  /// Return whether this number has gotten a full state yet.
  bool has_state() const { return has_state_; }

 protected:
  friend class NumberCall;

  /** Set the value of the number, this is a virtual method that each number integration must implement.
   *
   * This method is called by the NumberCall.
   *
   * @param value The value as validated by the NumberCall.
   */
  virtual void control(float value) = 0;

  uint32_t hash_base() override;

  CallbackManager<void(float)> state_callback_;
  bool has_state_{false};
};

}  // namespace number
}  // namespace esphome
