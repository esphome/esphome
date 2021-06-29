#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace number {

#define LOG_NUMBER(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, type, (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

class Number;

class NumberCall {
 public:
  explicit NumberCall(Number *parent) : parent_(parent) {}
  NumberCall &set_value(float value);
  void perform();

  const optional<float> &get_value() const;

 protected:
  Number *const parent_;
  optional<float> value_;
};

/** Base-class for all numbers.
 *
 * A number can use publish_state to send out a new value.
 */
class Number : public Nameable {
 public:
  explicit Number();
  explicit Number(const std::string &name);

  /** Manually set the icon of this number. By default the number's default defined by icon() is used.
   *
   * @param icon The icon, for example "mdi:flash". "" to disable.
   */
  void set_icon(const std::string &icon);
  /// Get the Home Assistant Icon. Uses the manual override if specified or the default value instead.
  std::string get_icon();

  /// Getter-syntax for .state.
  float get_state() const;

  /// Get the accuracy in decimals. Based on the step value.
  int8_t get_accuracy_decimals();

  /** Publish the current state to the front-end.
   */
  void publish_state(float state);

  NumberCall make_call();

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Add a callback that will be called every time the state changes.
  void add_on_state_callback(std::function<void(float)> &&callback);

  /** This member variable stores the last state.
   *
   * On startup, when no state is available yet, this is NAN (not-a-number) and the validity
   * can be checked using has_state().
   *
   * This is exposed through a member variable for ease of use in esphome lambdas.
   */
  float state;

  /// Return whether this number has gotten a full state yet.
  bool has_state() const;

  /** Set the value of the number, this is a virtual method that each number integration must implement.
   *
   * This method is called by the NumberCall.
   *
   * @param value The value as validated by the NumberCall.
   */
  virtual void set(float value) = 0;

  /// Return with which interval the number is polled. Return 0 for non-polling mode.
  virtual uint32_t update_interval();

  void set_min_value(float min_value) { this->min_value_ = min_value; }
  void set_max_value(float max_value) { this->max_value_ = max_value; }
  void set_step(float step) { this->step_ = step; }

  float min_value() const { return this->min_value_; }
  float max_value() const { return this->max_value_; }
  float step() const { return this->step_; }

 protected:
  uint32_t hash_base() override;

  CallbackManager<void(float)> state_callback_;
  /// Override the icon advertised to Home Assistant, otherwise number's icon will be used.
  optional<std::string> icon_;
  bool has_state_{false};
  float step_{1.0};
  float min_value_{0};
  float max_value_{100};
};

}  // namespace number
}  // namespace esphome
